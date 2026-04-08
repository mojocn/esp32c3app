#ifndef CRON_ENGINE_H
#define CRON_ENGINE_H

#include "cJSON.h"
#include <stdbool.h>
#include <stdint.h>

#define CRON_MAX_JOBS 16
#define CRON_EXPR_MAX_LEN 64
#define CRON_METHOD_MAX_LEN 64

typedef struct {
  uint8_t id;
  char expression[CRON_EXPR_MAX_LEN];
  char method[CRON_METHOD_MAX_LEN];
  cJSON *params;
  bool enabled;
} CronJob;

/* Initialize cron engine: loads persisted jobs, starts SNTP, launches scheduler task */
void cron_engine_init(void);

/* CRUD — all thread-safe and auto-persist to NVS */
int cron_job_create(const char *expression, const char *method, cJSON *params, bool enabled);
bool cron_job_update(uint8_t id, const char *expression, const char *method, cJSON *params, bool enabled);
bool cron_job_delete(uint8_t id);
cJSON *cron_job_list(void);
cJSON *cron_job_get(uint8_t id);

/* Validate a cron expression without creating a job (returns true if valid) */
bool cron_expr_validate(const char *expression);

#endif // CRON_ENGINE_H
