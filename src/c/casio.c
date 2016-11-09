/*
 * Casio CA-53W - The Retro Calculator Watch for the Pebble
 * Copyright (c) 2016 James Smith (jamessmithsmusic@gmail.com)
 * Licensed under the MIT license: http://opensource.org/licenses/MIT
 */

#include <pebble.h>

static Window *windowMain;

static bool appStarted = false;

static GBitmap     *bgBitmap;
static GBitmap     *btConIconBitmap, *btDisIconBitmap;
static GBitmap     *batFullIconBitmap, *batChargingIconBitmap, *batEmptyIconBitmap, *bat33IconBitmap, *bat66IconBitmap;
static BitmapLayer *bgLayer, *btIconLayer, *batIconLayer;

static GBitmap     *imageDay;
static BitmapLayer *layerDay;
const int IMAGE_DAY_RESOURCE_IDS[] = {
  RESOURCE_ID_IMAGE_DAY_SU,
  RESOURCE_ID_IMAGE_DAY_MO,
  RESOURCE_ID_IMAGE_DAY_TU,
  RESOURCE_ID_IMAGE_DAY_WE,
  RESOURCE_ID_IMAGE_DAY_TH,
  RESOURCE_ID_IMAGE_DAY_FR,
  RESOURCE_ID_IMAGE_DAY_SA
};

#define TOTAL_DIGITS 8
static GBitmap     *imagesTimeDigits[TOTAL_DIGITS], *imagesDateDigits[TOTAL_DIGITS];
static BitmapLayer *layersTimeDigits[TOTAL_DIGITS], *layersDateDigits[TOTAL_DIGITS];
const int IMAGE_NUM_RESOURCE_IDS[] = {
  RESOURCE_ID_IMAGE_NUM_0,
  RESOURCE_ID_IMAGE_NUM_1,
  RESOURCE_ID_IMAGE_NUM_2,
  RESOURCE_ID_IMAGE_NUM_3,
  RESOURCE_ID_IMAGE_NUM_4,
  RESOURCE_ID_IMAGE_NUM_5,
  RESOURCE_ID_IMAGE_NUM_6,
  RESOURCE_ID_IMAGE_NUM_7,
  RESOURCE_ID_IMAGE_NUM_8,
  RESOURCE_ID_IMAGE_NUM_9
};

static GBitmap     *imagePeriod;
static BitmapLayer *layerPeriod;

static void set_container_image(GBitmap **bmp_image, BitmapLayer *bmp_layer, const int resource_id, GPoint origin);
static void main_window_load(Window *window);
static void main_window_unload(Window *window);
static void handle_tick(struct tm *tick_time, TimeUnits units_changed);
static void handle_accel(AccelAxisType axis, int32_t direction);
static void callback_bluetooth(bool connected);
static void callback_battery(BatteryChargeState state);

static void set_container_image(GBitmap **bmp_image, BitmapLayer *bmp_layer, const int resource_id, GPoint origin) {
  //Temp variable for old image
  GBitmap *old_image = *bmp_image;

  //Replace old image with new image
  *bmp_image = gbitmap_create_with_resource(resource_id);
  GRect frame = (GRect) {
    .origin = origin,
    .size = gbitmap_get_bounds(*bmp_image).size
  };
  bitmap_layer_set_bitmap(bmp_layer, *bmp_image);
  layer_set_frame(bitmap_layer_get_layer(bmp_layer), frame);

  //Get rid of the old image if it exits
  if (old_image != NULL) {
    gbitmap_destroy(old_image);
  }
}

static void main_window_load(Window *window) {
  // Get the bounds of the Window
  Layer *windowLayer = window_get_root_layer(window);
  GRect windowBounds = layer_get_bounds(windowLayer);
  
  // Background
  bgBitmap = gbitmap_create_with_resource(RESOURCE_ID_IMAGE_BACKGROUND);
  bgLayer = bitmap_layer_create(windowBounds);
  bitmap_layer_set_bitmap(bgLayer, bgBitmap);
  layer_add_child(windowLayer, bitmap_layer_get_layer(bgLayer));
  
  // Bluetooth Icon
  btConIconBitmap = gbitmap_create_with_resource(RESOURCE_ID_IMAGE_ICON_BT_CON);
  btDisIconBitmap = gbitmap_create_with_resource(RESOURCE_ID_IMAGE_ICON_BT_DIS);
  btIconLayer = bitmap_layer_create(GRect(60, 25, 10, 10));
  bitmap_layer_set_bitmap(btIconLayer, btConIconBitmap);
  bitmap_layer_set_compositing_mode(btIconLayer, GCompOpSet);
  layer_add_child(window_get_root_layer(window), bitmap_layer_get_layer(btIconLayer));
  
  // Battery Icon
  batFullIconBitmap = gbitmap_create_with_resource(RESOURCE_ID_IMAGE_ICON_BAT_FULL);
  batChargingIconBitmap = gbitmap_create_with_resource(RESOURCE_ID_IMAGE_ICON_BAT_CHARGING);
  batEmptyIconBitmap = gbitmap_create_with_resource(RESOURCE_ID_IMAGE_ICON_BAT_EMPTY);
  bat33IconBitmap = gbitmap_create_with_resource(RESOURCE_ID_IMAGE_ICON_BAT_33);
  bat66IconBitmap = gbitmap_create_with_resource(RESOURCE_ID_IMAGE_ICON_BAT_66);
  batIconLayer = bitmap_layer_create(GRect(50, 25, 10, 10));
  bitmap_layer_set_bitmap(batIconLayer, batFullIconBitmap);
  bitmap_layer_set_compositing_mode(batIconLayer, GCompOpSet);
  layer_add_child(window_get_root_layer(window), bitmap_layer_get_layer(batIconLayer));
  
  // Time Layer, "12:34 56"
  memset(&layersTimeDigits, 0, sizeof(layersTimeDigits));
  memset(&imagesTimeDigits, 0, sizeof(imagesTimeDigits));
  for (int i = 0; i < TOTAL_DIGITS; i++) {
    layersTimeDigits[i] = bitmap_layer_create(windowBounds);
    bitmap_layer_set_compositing_mode(layersTimeDigits[i], GCompOpSet);
    layer_add_child(windowLayer, bitmap_layer_get_layer(layersTimeDigits[i]));
  }
  
  // Date Layer, "YY MM-DD", shake to show
  memset(&layersDateDigits, 0, sizeof(layersDateDigits));
  memset(&imagesDateDigits, 0, sizeof(imagesDateDigits));
  for (int i = 0; i < TOTAL_DIGITS; i++) {
    layersDateDigits[i] = bitmap_layer_create(windowBounds);
    bitmap_layer_set_compositing_mode(layersDateDigits[i], GCompOpSet);
    layer_add_child(windowLayer, bitmap_layer_get_layer(layersDateDigits[i]));
    layer_set_hidden(bitmap_layer_get_layer(layersDateDigits[i]), true);
  }
  
  // Day Layer, "SU" "MO" "TU" "WE" "TH" "FR" "SA"
  layerDay = bitmap_layer_create(windowBounds);
  bitmap_layer_set_compositing_mode(layerDay, GCompOpSet);
  layer_add_child(windowLayer, bitmap_layer_get_layer(layerDay));
  
  // Period Layer, "AM" "PM"
  layerPeriod = bitmap_layer_create(windowBounds);
  bitmap_layer_set_compositing_mode(layerPeriod, GCompOpSet);
  layer_add_child(windowLayer, bitmap_layer_get_layer(layerPeriod));
  
  // Show the correct state of the BT connection from the start
  callback_bluetooth(connection_service_peek_pebble_app_connection());
}

// Free allocated data.
static void main_window_unload(Window *window) {
  // Period Layer
  layer_remove_from_parent(bitmap_layer_get_layer(layerPeriod));
  bitmap_layer_destroy(layerPeriod);
  gbitmap_destroy(imagePeriod);
  
  // Day Layer
  layer_remove_from_parent(bitmap_layer_get_layer(layerDay));
  bitmap_layer_destroy(layerDay);
  gbitmap_destroy(imageDay);
  
  // Date Layer
  for (int i = 0; i < TOTAL_DIGITS; i++) {
    layer_remove_from_parent(bitmap_layer_get_layer(layersDateDigits[i]));
    gbitmap_destroy(imagesDateDigits[i]);
    bitmap_layer_destroy(layersDateDigits[i]);
  }
  
  // Time Layer
  for (int i = 0; i < TOTAL_DIGITS; i++) {
    layer_remove_from_parent(bitmap_layer_get_layer(layersTimeDigits[i]));
    gbitmap_destroy(imagesTimeDigits[i]);
    bitmap_layer_destroy(layersTimeDigits[i]);
  }
  
  // Destroy Battery Icon
  gbitmap_destroy(batFullIconBitmap);
  gbitmap_destroy(batChargingIconBitmap);
  gbitmap_destroy(batEmptyIconBitmap);
  gbitmap_destroy(bat33IconBitmap);
  gbitmap_destroy(bat66IconBitmap);
  bitmap_layer_destroy(batIconLayer);
  
  // Destroy Bluetooth Icon
  gbitmap_destroy(btConIconBitmap);
  gbitmap_destroy(btDisIconBitmap);
  bitmap_layer_destroy(btIconLayer);
  
  // Destroy Background
  bitmap_layer_destroy(bgLayer);
  gbitmap_destroy(bgBitmap);
}

static unsigned short get_display_hour(unsigned short hour) {
  if (clock_is_24h_style()) {
    return hour;
  }
  unsigned short display_hour = hour % 12;

  // Converts "0" to "12"
  return display_hour ? display_hour : 12;
}

static void update_days(struct tm *tick_time) {
  // Set the date with the format "YY MM-DD"
  set_container_image(&imagesDateDigits[0], layersDateDigits[0], IMAGE_NUM_RESOURCE_IDS[(tick_time->tm_year/10)%10], GPoint(21, 45));
  set_container_image(&imagesDateDigits[1], layersDateDigits[1], IMAGE_NUM_RESOURCE_IDS[tick_time->tm_year%10], GPoint(34, 45));
  set_container_image(&imagesDateDigits[3], layersDateDigits[3], IMAGE_NUM_RESOURCE_IDS[(tick_time->tm_mon+1)/10], GPoint(60, 45));
  set_container_image(&imagesDateDigits[4], layersDateDigits[4], IMAGE_NUM_RESOURCE_IDS[(tick_time->tm_mon+1)%10], GPoint(73, 45));
  set_container_image(&imagesDateDigits[5], layersDateDigits[5], RESOURCE_ID_IMAGE_NUM_DASH, GPoint(86, 45));
  set_container_image(&imagesDateDigits[6], layersDateDigits[6], IMAGE_NUM_RESOURCE_IDS[tick_time->tm_mday/10], GPoint(99, 45));
  set_container_image(&imagesDateDigits[7], layersDateDigits[7], IMAGE_NUM_RESOURCE_IDS[tick_time->tm_mday%10], GPoint(112, 45));
  
  // Set the day
  set_container_image(&imageDay, layerDay, IMAGE_DAY_RESOURCE_IDS[tick_time->tm_wday], GPoint(99, 27));
}

static void update_hours(struct tm *tick_time) {
  // Set the hours
  unsigned short displayHour = get_display_hour(tick_time->tm_hour);
  set_container_image(&imagesTimeDigits[0], layersTimeDigits[0], IMAGE_NUM_RESOURCE_IDS[displayHour/10], GPoint(21, 45));
  set_container_image(&imagesTimeDigits[1], layersTimeDigits[1], IMAGE_NUM_RESOURCE_IDS[displayHour%10], GPoint(34, 45));
  set_container_image(&imagesTimeDigits[2], layersTimeDigits[2], RESOURCE_ID_IMAGE_NUM_COLON, GPoint(47, 45));
  
  // Set the period
  if(tick_time->tm_hour < 12) {
    set_container_image(&imagePeriod, layerPeriod, RESOURCE_ID_IMAGE_PERIOD_AM, GPoint(21, 36));
  } else {
    set_container_image(&imagePeriod, layerPeriod, RESOURCE_ID_IMAGE_PERIOD_PM, GPoint(32, 36));
  }
}

static void update_minutes(struct tm *tick_time) {
  set_container_image(&imagesTimeDigits[3], layersTimeDigits[3], IMAGE_NUM_RESOURCE_IDS[tick_time->tm_min/10], GPoint(60, 45));
  set_container_image(&imagesTimeDigits[4], layersTimeDigits[4], IMAGE_NUM_RESOURCE_IDS[tick_time->tm_min%10], GPoint(73, 45));
}

static void update_seconds(struct tm *tick_time) {
  set_container_image(&imagesTimeDigits[6], layersTimeDigits[6], IMAGE_NUM_RESOURCE_IDS[tick_time->tm_sec/10], GPoint(99, 45));
  set_container_image(&imagesTimeDigits[7], layersTimeDigits[7], IMAGE_NUM_RESOURCE_IDS[tick_time->tm_sec%10], GPoint(112, 45));
}

static void handle_tick(struct tm *tick_time, TimeUnits units_changed) {
  if (units_changed & DAY_UNIT) {
    update_days(tick_time);
  }
  if (units_changed & HOUR_UNIT) {
    update_hours(tick_time);
  }
  if (units_changed & MINUTE_UNIT) {
    update_minutes(tick_time);
  }
  if (units_changed & SECOND_UNIT) {
    update_seconds(tick_time);
  }
}

static void handle_switch() {
  // Switch to time layer if it is currently hidden.
  if(layer_get_hidden(bitmap_layer_get_layer(layersTimeDigits[0]))) {
    for (int i = 0; i < TOTAL_DIGITS; i++) {
      layer_set_hidden(bitmap_layer_get_layer(layersTimeDigits[i]), false);
      layer_set_hidden(bitmap_layer_get_layer(layersDateDigits[i]), true);
    }
  } else {
    for (int i = 0; i < TOTAL_DIGITS; i++) {
      layer_set_hidden(bitmap_layer_get_layer(layersTimeDigits[i]), true);
      layer_set_hidden(bitmap_layer_get_layer(layersDateDigits[i]), false);
    }
  }
}

static void handle_accel(AccelAxisType axis, int32_t direction) {
  // A tap event occured
  handle_switch();
  app_timer_register(3000, handle_switch, NULL);
}

static void callback_bluetooth(bool connected) {
  if(!connected) {
    bitmap_layer_set_bitmap(btIconLayer, btDisIconBitmap);
    // Issue a vibrating alert
    if (appStarted) {
      vibes_double_pulse();
    }
  } else {
    bitmap_layer_set_bitmap(btIconLayer, btConIconBitmap);
  }
}

static void callback_battery(BatteryChargeState state) {
  if(state.is_charging) {
    bitmap_layer_set_bitmap(batIconLayer, batChargingIconBitmap);
    return;
  } else {
    // Change battery icon based of battery charge percent
    if(state.charge_percent == 0) {
      bitmap_layer_set_bitmap(batIconLayer, batEmptyIconBitmap);
    } else if(state.charge_percent <= 33) {
      bitmap_layer_set_bitmap(batIconLayer, bat33IconBitmap);
    } else if(state.charge_percent <= 66) {
      bitmap_layer_set_bitmap(batIconLayer, bat66IconBitmap);
    } else if (state.charge_percent <= 100) {
      bitmap_layer_set_bitmap(batIconLayer, batFullIconBitmap);
    }
  }
}

void init(void) {
  // Create Window 'windowMain'
  windowMain = window_create();
  window_set_background_color(windowMain, GColorBlack);
  window_set_window_handlers(windowMain, (WindowHandlers) {
    .load   = main_window_load,
    .unload = main_window_unload,
  });
  window_stack_push(windowMain, true);
  
  // Avoids a blank screen on watch start.
  time_t now = time(NULL);
  struct tm *tick_time = localtime(&now);  
  handle_tick(tick_time, DAY_UNIT + HOUR_UNIT + MINUTE_UNIT + SECOND_UNIT);
  // Ensure battery level is displayed from the start
  callback_battery(battery_state_service_peek());

  tick_timer_service_subscribe(SECOND_UNIT, handle_tick);
  accel_tap_service_subscribe(handle_accel);
  
  // Register for Bluetooth connection updates
  connection_service_subscribe((ConnectionHandlers) {
    .pebble_app_connection_handler = callback_bluetooth
  });
  
  // Register for battery level updates
  battery_state_service_subscribe(callback_battery);
  
  appStarted = true;
}

void deinit(void) {
  battery_state_service_unsubscribe();
  connection_service_unsubscribe();
  accel_tap_service_unsubscribe();
  tick_timer_service_unsubscribe();
  window_destroy(windowMain);
}

int main(void) {
  init();
  app_event_loop();
  deinit();
}