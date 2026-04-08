#include "cron_engine.h"

#include "cJSON.h"
#include "esp_log.h"
#include "esp_sntp.h"
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"
#include "freertos/task.h"
#include "rpc_m.h"

#include <nvs_flash.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

static const char *TAG = "CRON";
static const char *NVS_NAMESPACE = "cron";
static const char *NVS_KEY = "jobs";

#define CRON_CHECK_INTERVAL_S 30
#define CRON_TASK_STACK_SIZE 4096
#define CRON_TZ "CST-8"

/* ---- Parsed schedule bitmasks ---- */
typedef struct {
  uint64_t minutes; /* bits 0-59 */
  uint32_t hours;   /* bits 0-23 */
  uint32_t days;    /* bits 1-31 */
  uint16_t months;  /* bits 1-12 */
  uint8_t weekdays; /* bits 0-6, 0=Sunday */
} CronSchedule;

/* ---- State ---- */
static CronJob jobs[CRON_MAX_JOBS];
static int job_count = 0;
static uint8_t next_id = 1;
static SemaphoreHandle_t cron_mutex = NULL;

/* ---- Forward declarations ---- */
static bool parse_cron_expression(const char *expr, CronSchedule *out);
static bool cron_matches(const CronSchedule *s, const struct tm *t);
static void cron_task(void *arg);
static void cron_load(void);
static void cron_save(void);

/* ================================================================
 *  Cron expression parser
 *  Supports: * , - / and combinations  (5-field: min hour dom month dow)
 * ================================================================ */

static bool parse_subfield(const char *s, int len, uint64_t *bits, int min, int max) {
  char buf[20];
  if (len <= 0 || len >= (int)sizeof(buf)) return false;
  memcpy(buf, s, len);
  buf[len] = '\0';

  int step = 0;
  char *slash = strchr(buf, '/');
  if (slash) {
    *slash = '\0';
    step = atoi(slash + 1);
    if (step <= 0) return false;
  }

  if (buf[0] == '*') {
    if (step == 0) step = 1;
    for (int i = min; i <= max; i += step)
      *bits |= (1ULL << i);
    return true;
  }

  char *dash = strchr(buf, '-');
  if (dash) {
    *dash = '\0';
    int lo = atoi(buf);
    int hi = atoi(dash + 1);
    if (lo < min || hi > max || lo > hi) return false;
    if (step == 0) step = 1;
    for (int i = lo; i <= hi; i += step)
      *bits |= (1ULL << i);
    return true;
  }

  int val = atoi(buf);
  if (val < min || val > max) return false;
  if (step > 0) {
    for (int i = val; i <= max; i += step)
      *bits |= (1ULL << i);
  } else {
    *bits |= (1ULL << val);
  }
  return true;
}

static bool parse_field(const char *field, uint64_t *bits, int min, int max) {
  *bits = 0;
  const char *p = field;
  while (*p) {
    const char *comma = strchr(p, ',');
    int len = comma ? (int)(comma - p) : (int)strlen(p);
    if (!parse_subfield(p, len, bits, min, max)) return false;
    if (comma)
      p = comma + 1;
    else
      break;
  }
  return *bits != 0;
}

static bool parse_cron_expression(const char *expr, CronSchedule *out) {
  char buf[CRON_EXPR_MAX_LEN];
  strncpy(buf, expr, sizeof(buf) - 1);
  buf[sizeof(buf) - 1] = '\0';

  char *fields[5];
  char *tok = strtok(buf, " \t");
  for (int i = 0; i < 5; i++) {
    if (!tok) return false;
    fields[i] = tok;
    tok = strtok(NULL, " \t");
  }

  uint64_t bits;
  if (!parse_field(fields[0], &bits, 0, 59)) return false;
  out->minutes = bits;

  if (!parse_field(fields[1], &bits, 0, 23)) return false;
  out->hours = (uint32_t)bits;

  if (!parse_field(fields[2], &bits, 1, 31)) return false;
  out->days = (uint32_t)bits;

  if (!parse_field(fields[3], &bits, 1, 12)) return false;
  out->months = (uint16_t)bits;

  if (!parse_field(fields[4], &bits, 0, 6)) return false;
  out->weekdays = (uint8_t)bits;

  return true;
}

bool cron_expr_validate(const char *expression) {
  CronSchedule tmp;
  return parse_cron_expression(expression, &tmp);
}

/* ================================================================
 *  Time matching
 * ================================================================ */

static bool cron_matches(const CronSchedule *s, const struct tm *t) {
  if (!(s->minutes & (1ULL << t->tm_min))) return false;
  if (!(s->hours & (1UL << t->tm_hour))) return false;
  if (!(s->days & (1UL << t->tm_mday))) return false;
  if (!(s->months & (1U << (t->tm_mon + 1)))) return false;
  if (!(s->weekdays & (1U << t->tm_wday))) return false;
  return true;
}

/* ================================================================
 *  NVS persistence
 * ================================================================ */

static void cron_save(void) {
  cJSON *arr = cJSON_CreateArray();
  for (int i = 0; i < job_count; i++) {
    cJSON *obj = cJSON_CreateObject();
    cJSON_AddNumberToObject(obj, "id", jobs[i].id);
    cJSON_AddStringToObject(obj, "expression", jobs[i].expression);
    cJSON_AddStringToObject(obj, "method", jobs[i].method);
    cJSON_AddBoolToObject(obj, "enabled", jobs[i].enabled);
    if (jobs[i].params) {
      cJSON_AddItemToObject(obj, "params", cJSON_Duplicate(jobs[i].params, true));
    }
    cJSON_AddItemToArray(arr, obj);
  }

  char *json_str = cJSON_PrintUnformatted(arr);
  cJSON_Delete(arr);
  if (!json_str) return;

  nvs_handle_t handle;
  if (nvs_open(NVS_NAMESPACE, NVS_READWRITE, &handle) == ESP_OK) {
    nvs_set_str(handle, NVS_KEY, json_str);
    nvs_commit(handle);
    nvs_close(handle);
  }
  free(json_str);
}

static void cron_load(void) {
  nvs_handle_t handle;
  if (nvs_open(NVS_NAMESPACE, NVS_READONLY, &handle) != ESP_OK) return;

  size_t required = 0;
  if (nvs_get_str(handle, NVS_KEY, NULL, &required) != ESP_OK || required == 0) {
    nvs_close(handle);
    return;
  }

  char *json_str = malloc(required);
  if (!json_str) {
    nvs_close(handle);
    return;
  }
  if (nvs_get_str(handle, NVS_KEY, json_str, &required) != ESP_OK) {
    free(json_str);
    nvs_close(handle);
    return;
  }
  nvs_close(handle);

  cJSON *arr = cJSON_Parse(json_str);
  free(json_str);
  if (!arr || !cJSON_IsArray(arr)) {
    cJSON_Delete(arr);
    return;
  }

  int count = cJSON_GetArraySize(arr);
  job_count = 0;
  next_id = 1;

  for (int i = 0; i < count && job_count < CRON_MAX_JOBS; i++) {
    cJSON *obj = cJSON_GetArrayItem(arr, i);
    cJSON *jid = cJSON_GetObjectItem(obj, "id");
    cJSON *jexpr = cJSON_GetObjectItem(obj, "expression");
    cJSON *jmethod = cJSON_GetObjectItem(obj, "method");
    cJSON *jenabled = cJSON_GetObjectItem(obj, "enabled");
    cJSON *jparams = cJSON_GetObjectItem(obj, "params");

    if (!cJSON_IsNumber(jid) || !cJSON_IsString(jexpr) || !cJSON_IsString(jmethod)) continue;

    CronJob *job = &jobs[job_count];
    job->id = (uint8_t)jid->valueint;
    strncpy(job->expression, jexpr->valuestring, CRON_EXPR_MAX_LEN - 1);
    job->expression[CRON_EXPR_MAX_LEN - 1] = '\0';
    strncpy(job->method, jmethod->valuestring, CRON_METHOD_MAX_LEN - 1);
    job->method[CRON_METHOD_MAX_LEN - 1] = '\0';
    job->enabled = cJSON_IsBool(jenabled) ? cJSON_IsTrue(jenabled) : true;
    job->params = jparams ? cJSON_Duplicate(jparams, true) : NULL;

    if (job->id >= next_id) next_id = job->id + 1;
    job_count++;
  }

  cJSON_Delete(arr);
  ESP_LOGI(TAG, "Loaded %d cron job(s)", job_count);
}

/* ================================================================
 *  CRUD operations (caller must NOT hold mutex)
 * ================================================================ */

static cJSON *job_to_json(const CronJob *job) {
  cJSON *obj = cJSON_CreateObject();
  cJSON_AddNumberToObject(obj, "id", job->id);
  cJSON_AddStringToObject(obj, "expression", job->expression);
  cJSON_AddStringToObject(obj, "method", job->method);
  cJSON_AddBoolToObject(obj, "enabled", job->enabled);
  if (job->params) cJSON_AddItemToObject(obj, "params", cJSON_Duplicate(job->params, true));
  return obj;
}

int cron_job_create(const char *expression, const char *method, cJSON *params, bool enabled) {
  CronSchedule tmp;
  if (!parse_cron_expression(expression, &tmp)) return -1;

  xSemaphoreTake(cron_mutex, portMAX_DELAY);
  if (job_count >= CRON_MAX_JOBS) {
    xSemaphoreGive(cron_mutex);
    return -2;
  }

  CronJob *job = &jobs[job_count];
  job->id = next_id++;
  if (next_id == 0) next_id = 1; /* wrap-around guard */
  strncpy(job->expression, expression, CRON_EXPR_MAX_LEN - 1);
  job->expression[CRON_EXPR_MAX_LEN - 1] = '\0';
  strncpy(job->method, method, CRON_METHOD_MAX_LEN - 1);
  job->method[CRON_METHOD_MAX_LEN - 1] = '\0';
  job->enabled = enabled;
  job->params = params ? cJSON_Duplicate(params, true) : NULL;
  job_count++;

  int id = job->id;
  cron_save();
  xSemaphoreGive(cron_mutex);

  ESP_LOGI(TAG, "Created job %d: [%s] -> %s", id, expression, method);
  return id;
}

bool cron_job_update(uint8_t id, const char *expression, const char *method, cJSON *params, bool enabled) {
  if (expression) {
    CronSchedule tmp;
    if (!parse_cron_expression(expression, &tmp)) return false;
  }

  xSemaphoreTake(cron_mutex, portMAX_DELAY);
  for (int i = 0; i < job_count; i++) {
    if (jobs[i].id != id) continue;

    if (expression) {
      strncpy(jobs[i].expression, expression, CRON_EXPR_MAX_LEN - 1);
      jobs[i].expression[CRON_EXPR_MAX_LEN - 1] = '\0';
    }
    if (method) {
      strncpy(jobs[i].method, method, CRON_METHOD_MAX_LEN - 1);
      jobs[i].method[CRON_METHOD_MAX_LEN - 1] = '\0';
    }
    if (params) {
      if (jobs[i].params) cJSON_Delete(jobs[i].params);
      jobs[i].params = cJSON_Duplicate(params, true);
    }
    jobs[i].enabled = enabled;

    cron_save();
    xSemaphoreGive(cron_mutex);
    ESP_LOGI(TAG, "Updated job %d", id);
    return true;
  }
  xSemaphoreGive(cron_mutex);
  return false;
}

bool cron_job_delete(uint8_t id) {
  xSemaphoreTake(cron_mutex, portMAX_DELAY);
  for (int i = 0; i < job_count; i++) {
    if (jobs[i].id != id) continue;

    if (jobs[i].params) cJSON_Delete(jobs[i].params);

    for (int j = i; j < job_count - 1; j++)
      jobs[j] = jobs[j + 1];
    job_count--;
    memset(&jobs[job_count], 0, sizeof(CronJob));

    cron_save();
    xSemaphoreGive(cron_mutex);
    ESP_LOGI(TAG, "Deleted job %d", id);
    return true;
  }
  xSemaphoreGive(cron_mutex);
  return false;
}

cJSON *cron_job_list(void) {
  cJSON *arr = cJSON_CreateArray();
  xSemaphoreTake(cron_mutex, portMAX_DELAY);
  for (int i = 0; i < job_count; i++)
    cJSON_AddItemToArray(arr, job_to_json(&jobs[i]));
  xSemaphoreGive(cron_mutex);
  return arr;
}

cJSON *cron_job_get(uint8_t id) {
  xSemaphoreTake(cron_mutex, portMAX_DELAY);
  for (int i = 0; i < job_count; i++) {
    if (jobs[i].id == id) {
      cJSON *obj = job_to_json(&jobs[i]);
      xSemaphoreGive(cron_mutex);
      return obj;
    }
  }
  xSemaphoreGive(cron_mutex);
  return NULL;
}

/* ================================================================
 *  Scheduler task — checks every CRON_CHECK_INTERVAL_S seconds
 * ================================================================ */

static void cron_task(void *arg) {
  (void)arg;
  int last_exec_minute = -1;

  while (1) {
    vTaskDelay(pdMS_TO_TICKS(CRON_CHECK_INTERVAL_S * 1000));

    time_t now;
    time(&now);
    struct tm ti;
    localtime_r(&now, &ti);

    /* Skip if SNTP has not synchronised yet */
    if (ti.tm_year < (2024 - 1900)) continue;

    /* Execute at most once per calendar minute */
    int cur_min = ti.tm_year * 525960 + ti.tm_yday * 1440 + ti.tm_hour * 60 + ti.tm_min;
    if (cur_min == last_exec_minute) continue;
    last_exec_minute = cur_min;

    xSemaphoreTake(cron_mutex, portMAX_DELAY);
    for (int i = 0; i < job_count; i++) {
      if (!jobs[i].enabled) continue;

      CronSchedule sched;
      if (!parse_cron_expression(jobs[i].expression, &sched)) continue;

      if (!cron_matches(&sched, &ti)) continue;

      ESP_LOGI(TAG, "Firing job %d: %s(%s)", jobs[i].id, jobs[i].method, jobs[i].expression);

      /* Build a JSON-RPC request and dispatch it */
      cJSON *req = cJSON_CreateObject();
      cJSON_AddStringToObject(req, "jsonrpc", "2.0");
      cJSON_AddStringToObject(req, "method", jobs[i].method);
      cJSON_AddNumberToObject(req, "id", jobs[i].id);
      if (jobs[i].params) cJSON_AddItemToObject(req, "params", cJSON_Duplicate(jobs[i].params, true));

      char *req_str = cJSON_PrintUnformatted(req);
      cJSON_Delete(req);

      if (req_str) {
        /* Release mutex while executing so RPC handlers can't deadlock */
        xSemaphoreGive(cron_mutex);

        char *resp_str = rpc_process_request(req_str);
        ESP_LOGI(TAG, "Job %d result: %.128s", jobs[i].id, resp_str ? resp_str : "(null)");
        free(resp_str);
        free(req_str);

        /* Re-acquire – job list may have changed, restart scan */
        xSemaphoreTake(cron_mutex, portMAX_DELAY);
        break; /* restart loop next tick to avoid stale index */
      }
    }
    xSemaphoreGive(cron_mutex);
  }
}

/* ================================================================
 *  Initialisation
 * ================================================================ */

static void sntp_init_time(void) {
  if (esp_sntp_enabled()) return;

  ESP_LOGI(TAG, "Initialising SNTP (TZ=%s)", CRON_TZ);
  setenv("TZ", CRON_TZ, 1);
  tzset();

  esp_sntp_setoperatingmode(ESP_SNTP_OPMODE_POLL);
  esp_sntp_setservername(0, "pool.ntp.org");
  esp_sntp_setservername(1, "time.nist.org");
  esp_sntp_init();
}

void cron_engine_init(void) {
  cron_mutex = xSemaphoreCreateMutex();
  configASSERT(cron_mutex);

  cron_load();
  sntp_init_time();

  xTaskCreate(cron_task, "cron", CRON_TASK_STACK_SIZE, NULL, 5, NULL);
  ESP_LOGI(TAG, "Cron engine started (%d job(s) loaded)", job_count);
}
