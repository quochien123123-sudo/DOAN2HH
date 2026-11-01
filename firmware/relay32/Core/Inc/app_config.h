#ifndef SLAVE_APP_CONFIG_H
#define SLAVE_APP_CONFIG_H
/* Build the SAME slave sources with NODE_ID=1, 2 or 3. Never use duplicate IDs. */
#ifndef NODE_ID
#define NODE_ID 1U
#endif
#if NODE_ID < 1 || NODE_ID > 3
#error "NODE_ID must be 1, 2 or 3"
#endif
#ifndef SOIL_DRY_VALUE
#define SOIL_DRY_VALUE 3000U
#endif
#ifndef SOIL_WET_VALUE
#define SOIL_WET_VALUE 1500U
#endif
#define SENSOR_PERIOD_MS 1000U
#define BUTTON_DEBOUNCE_MS 40U
#define RADIO_RETRY_MS 5000U
#endif
