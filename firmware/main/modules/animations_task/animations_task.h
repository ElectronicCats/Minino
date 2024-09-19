#pragma once
#include <stdio.h>

/**
 * @brief Run the given animation callback every `period_ms` milliseconds.
 *
 * @param animation_cb The callback to run.
 * @param period_ms The period in milliseconds.
 * @param pvParameters The parameters to pass to the callback.
 *
 * @return void
 */
void animations_task_run(void* animation_cb,
                         uint32_t period_ms,
                         void* pvParameters);

/**
 * @brief Stop the animations task.
 *
 * @return void
 */
void animations_task_stop();