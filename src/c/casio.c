/*
 * Casio CA-53W - The Retro Calculator Watch for the Pebble
 * Copyright (c) 2016 James Smith (jamessmithsmusic@gmail.com)
 * Licensed under the MIT license: http://opensource.org/licenses/MIT
 */

#include <pebble.h>

static Window *windowMain;

static GBitmap     *bgBitmap;
static GBitmap     *btConIconBitmap, *btDisIconBitmap;
static GBitmap     *batFullIconBitmap, *batChargingIconBitmap, *batEmptyIconBitmap, *bat33IconBitmap, *bat66IconBitmap;
static BitmapLayer *bgLayer, *btIconLayer, *batIconLayer;

static TextLayer *layerTime;   // "12:34 56"
static TextLayer *layerDate;   // "YY MM-DD"
static TextLayer *layerDay;    // "SU" "MO" "TU" "WE" "TH" "FR" "SA"
static TextLayer *layerPeriod; // "AM" "PM"

static GFont fontTime;
static GFont fontDay;
static GFont fontPeriod;

static void main_window_load(Window *window);
static void main_window_unload(Window *window);
static void update_time(void);
static void handle_timer(struct tm *tick_time, TimeUnits units_changed);
static void handle_accel(AccelAxisType axis, int32_t direction);
static void callback_bluetooth(bool connected);
static void callback_battery(BatteryChargeState state);

static void main_window_load(Window *window) {
  // Get the bounds of the Window
  Layer *windowLayer = window_get_root_layer(window);
  GRect windowBounds = layer_get_bounds(windowLayer);
  
  // Font
  fontTime = fonts_load_custom_font(resource_get_handle(RESOURCE_ID_FONT_DIGITAL_28));
  fontDay = fonts_load_custom_font(resource_get_handle(RESOURCE_ID_FONT_DIGITAL_20));
  fontPeriod = fonts_load_custom_font(resource_get_handle(RESOURCE_ID_FONT_DROID_10));
  
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
  
  // Time Layer
  layerTime = text_layer_create(GRect(0, 35, windowBounds.size.w, 50));
  text_layer_set_font(layerTime, fontTime);
  text_layer_set_text_alignment(layerTime, GTextAlignmentCenter);
  text_layer_set_background_color(layerTime, GColorClear);
  text_layer_set_text_color(layerTime, GColorBlack);
  text_layer_set_text(layerTime, "00:00 00"); // Must hold 8 characters
  layer_add_child(windowLayer, text_layer_get_layer(layerTime));
  
  // Date Layer, Shake to show
  layerDate = text_layer_create(GRect(0, 35, windowBounds.size.w, 50));
  text_layer_set_font(layerDate, fontTime);
  text_layer_set_text_alignment(layerDate, GTextAlignmentCenter);
  text_layer_set_background_color(layerDate, GColorClear);
  text_layer_set_text_color(layerDate, GColorBlack);
  text_layer_set_text(layerDate, "YY MM-DD"); // Must hold 8 characters
  layer_add_child(windowLayer, text_layer_get_layer(layerDate));
  layer_set_hidden(text_layer_get_layer(layerDate), true);
  
  // Day Layer
  layerDay = text_layer_create(GRect(0, 20, windowBounds.size.w - 24, 25));
  text_layer_set_font(layerDay, fontDay);
  text_layer_set_text_alignment(layerDay, GTextAlignmentRight);
  text_layer_set_background_color(layerDay, GColorClear);
  text_layer_set_text_color(layerDay, GColorBlack);
  text_layer_set_text(layerDay, "SU");
  layer_add_child(windowLayer, text_layer_get_layer(layerDay));
  
  // Period Layer
  layerPeriod = text_layer_create(GRect(21, 32, windowBounds.size.w - 21, 25));
  text_layer_set_font(layerPeriod, fontPeriod);
  text_layer_set_text_alignment(layerPeriod, GTextAlignmentLeft);
  text_layer_set_background_color(layerPeriod, GColorClear);
  text_layer_set_text_color(layerPeriod, GColorBlack);
  text_layer_set_text(layerPeriod, "AMPM");
  layer_add_child(windowLayer, text_layer_get_layer(layerPeriod));
  
  // Show the correct state of the BT connection from the start
  callback_bluetooth(connection_service_peek_pebble_app_connection());
}

// Free allocated data.
static void main_window_unload(Window *window) {
  // Destroy Text Layers
  text_layer_destroy(layerPeriod);
  text_layer_destroy(layerDay);
  text_layer_destroy(layerDate);
  text_layer_destroy(layerTime);
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
  // Destroy Custom Font
  fonts_unload_custom_font(fontPeriod);
  fonts_unload_custom_font(fontDay);
  fonts_unload_custom_font(fontTime);
}

static void update_time(void) {
  // Get a tm structure
  time_t temp = time(NULL);
  struct tm *tickTime = localtime(&temp);

  // Set the current hour, minute, and second
  static char bufferTime[9];
  strftime(bufferTime, sizeof(bufferTime), clock_is_24h_style() ?
                                          "%H:%M %S" : "%I:%M %S", tickTime);
  text_layer_set_text(layerTime, bufferTime);
  
  // Set the Date
  static char bufferDate[9];
  strftime(bufferDate, sizeof(bufferDate), "%y %m-%d", tickTime);
  text_layer_set_text(layerDate, bufferDate);
  
  // Set the current day
  static char bufferDay[3];
  switch(tickTime->tm_wday) {
    case 0:
      strncpy(bufferDay, "SU", sizeof(bufferDay));
      break;
    case 1:
      strncpy(bufferDay, "MO", sizeof(bufferDay));
      break;
    case 2:
      strncpy(bufferDay, "TU", sizeof(bufferDay));
      break;
    case 3:
      strncpy(bufferDay, "WE", sizeof(bufferDay));
      break;
    case 4:
      strncpy(bufferDay, "TH", sizeof(bufferDay));
      break;
    case 5:
      strncpy(bufferDay, "FR", sizeof(bufferDay));
      break;
    case 6:
      strncpy(bufferDay, "SA", sizeof(bufferDay));
      break;
    default:
      break;
  }
  text_layer_set_text(layerDay, bufferDay);
  
  // Set the current period
  static char bufferPeriod[5];
  if(tickTime->tm_hour < 12) {
    strncpy(bufferPeriod, "AM  ", sizeof(bufferPeriod));
  } else {
    strncpy(bufferPeriod, "  PM", sizeof(bufferPeriod));
  }
  text_layer_set_text(layerPeriod, bufferPeriod);
}

static void handle_timer(struct tm *tick_time, TimeUnits units_changed) {
  update_time();
}

static void handle_switch() {
  if(layer_get_hidden(text_layer_get_layer(layerTime))) {
    layer_set_hidden(text_layer_get_layer(layerTime), false);
    layer_set_hidden(text_layer_get_layer(layerDate), true);
  } else {
    layer_set_hidden(text_layer_get_layer(layerTime), true);
    layer_set_hidden(text_layer_get_layer(layerDate), false);
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
    vibes_double_pulse();
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
  
  update_time();
  // Ensure battery level is displayed from the start
  callback_battery(battery_state_service_peek());

  tick_timer_service_subscribe(SECOND_UNIT, handle_timer);
  accel_tap_service_subscribe(handle_accel);
  
  // Register for Bluetooth connection updates
  connection_service_subscribe((ConnectionHandlers) {
    .pebble_app_connection_handler = callback_bluetooth
  });
  
  // Register for battery level updates
  battery_state_service_subscribe(callback_battery);
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