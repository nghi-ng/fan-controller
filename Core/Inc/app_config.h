#ifndef APP_CONFIG_H
#define APP_CONFIG_H

/*
 * Set to 1 for the priority comparison test.
 * Set to 0 for the final real-time demo build.
 */
#define RT_MEASURE_LOW_PRIORITY_TEST  0U

/*
 * Set to 1 when collecting latency CSV evidence.
 * Set to 0 to keep TIM2 measurement running without printing CSV rows.
 */
#define RT_MEASURE_LOG_CSV            0U

#if RT_MEASURE_LOW_PRIORITY_TEST
#define APP_FIRMWARE_MARKER "firmware,smartfan,low_priority_measurement_test\r\n"
#else
#define APP_FIRMWARE_MARKER "firmware,smartfan,final_realtime_priority\r\n"
#endif

#endif /* APP_CONFIG_H */
