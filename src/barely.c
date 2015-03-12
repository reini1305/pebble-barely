#include <pebble.h>

#define PERSIST_INVERTED 1
#define PERSIST_BLUETOOTH 2
#define TOP_LEFT 0
#define TOP_RIGHT 1
#define BOTTOM_LEFT 2
#define BOTTOM_RIGHT 3

static Window *window;
static Layer *canvas;

static Layer *digit_layers[4];
int digits[4];

static Layer *line_layers[5*4]; // max 5 layers per digit
static PropertyAnimation *line_animations[5*4];
static AppTimer *wait_timer;

bool invertInterface = false;
bool bluetoothIndicator = false;
bool bluetoothConnection = true;
bool animationRunning = false;

static void wait_timer_callback(void* data) {
  for (int i=0;i<4;layer_add_child(window_get_root_layer(window),digit_layers[i++])); // wait a second before we show anything
}

static unsigned short get_display_hour(unsigned short hour) {
	if (clock_is_24h_style()) {
		return hour;
	}
	unsigned short display_hour = hour % 12;
	return display_hour ? display_hour : 12;
}

static void animation_stopped(PropertyAnimation *animation, bool finished, void *data) {
#ifndef PBL_COLOR
  property_animation_destroy(animation);
  animation=NULL;
  //APP_LOG(APP_LOG_LEVEL_DEBUG,"Destroy called!");
#endif
}

static void animate_layer_bounds(int layerid, GRect fromRect, GRect toRect)
{
#ifdef PBL_BASALT
  if(animation_is_scheduled(property_animation_get_animation(line_animations[layerid]))) // already running?
    return;
#else
  if(animation_is_scheduled((Animation*)(line_animations[layerid]))) // already running?
    return;
#endif
  
  line_animations[layerid] = property_animation_create_layer_frame(line_layers[layerid], &fromRect, &toRect);
  animation_set_handlers((Animation*) line_animations[layerid], (AnimationHandlers) {
    .stopped = (AnimationStoppedHandler) animation_stopped,
  }, NULL);
  animation_set_duration((Animation*)line_animations[layerid],2000);
  animation_set_curve((Animation*)line_animations[layerid],AnimationCurveEaseInOut);
  //APP_LOG(APP_LOG_LEVEL_DEBUG,"animation running...");
  animation_schedule((Animation*)line_animations[layerid]);
}

void drawLine(GContext* ctx, GPoint start, GPoint goal, GPoint offset, int layerid) {
  GRect from;
  GRect to;
  
  // check if vertical or horizontal
  bool horizontal = (start.y == goal.y);
  
  // animation starts in between start and goal
  if(horizontal) {
    from.origin.x = (goal.x - start.x) / 2 + start.x + offset.x;
    from.origin.y = start.y - 1 + offset.y;
    from.size.w = 0;
    from.size.h = 3;
  } else {
    from.origin.x = start.x - 1 + offset.x;
    from.origin.y = (goal.y - start.y) / 2 + start.y - 1 + offset.y;
    from.size.w = 3;
    from.size.h = 0;
  }
  // animation ends in start and goal
  if(horizontal) {
    to.origin.x = start.x + offset.x;
    to.origin.y = start.y - 1 + offset.y;
    to.size.w = goal.x - start.x ;
    to.size.h = 3;
  } else {
    to.origin.x = start.x - 1 + offset.x;
    to.origin.y = start.y - 1 + offset.y;
    to.size.w = 3;
    to.size.h = goal.y - start.y + 3;
  }
  //layer_set_frame(line_layers[layerid],from);
  //to=to;
  animate_layer_bounds(layerid, from, to);
}

void drawLine_callback(Layer *layer, GContext* ctx) {
  if(invertInterface)
    graphics_context_set_fill_color(ctx,GColorWhite);
  else
    graphics_context_set_fill_color(ctx,GColorBlack);
  //GRect bounds = layer_get_frame(layer);
  graphics_fill_rect(ctx,GRect(0,0,144,168),0,GCornerNone);
}

void renderNumber(int number, GContext* ctx, unsigned char quadrant) {
  GPoint offset;
  unsigned int line_layer_offset=0;
  if (invertInterface) {
    graphics_context_set_fill_color(ctx, GColorWhite);
  }
  switch (quadrant) {
    case TOP_LEFT:
      offset.x=0;
      offset.y=0;
      break;
    case TOP_RIGHT:
      offset.x=73;
      offset.y=0;
      line_layer_offset=5;
      break;
    case BOTTOM_LEFT:
      offset.x=0;
      offset.y=85;
      line_layer_offset=10;
      break;
    case BOTTOM_RIGHT:
      offset.x=73;
      offset.y=85;
      line_layer_offset=15;
      break;
  }
  // reset line layers
  for (int i=0; i<5; layer_set_frame(line_layers[line_layer_offset+i],GRect(0,0,0,0)),i++);
  
  // draw new line layers
  //offset=offset;
  if (number == 1) {
		drawLine(ctx, GPoint(0,26), GPoint(22,26), offset, line_layer_offset);
		drawLine(ctx, GPoint(22,26), GPoint(22,55), offset, line_layer_offset+1);
		drawLine(ctx, GPoint(0,55), GPoint(22,55), offset, line_layer_offset+2);
		drawLine(ctx, GPoint(47,0), GPoint(47,55), offset, line_layer_offset+3);
		drawLine(ctx, GPoint(47,55), GPoint(71,55), offset, line_layer_offset+4);
	} else if (number == 2) {
		drawLine(ctx, GPoint(0,26), GPoint(47,26), offset, line_layer_offset);
		drawLine(ctx, GPoint(22,55), GPoint(71,55), offset, line_layer_offset+1);
	} else if (number == 3) {
		drawLine(ctx, GPoint(0,26), GPoint(47,26), offset, line_layer_offset);
		drawLine(ctx, GPoint(0,55), GPoint(47,55), offset, line_layer_offset+1);
	} else if (number == 4) {
		drawLine(ctx, GPoint(35,0), GPoint(35,26), offset, line_layer_offset);
		drawLine(ctx, GPoint(0,55), GPoint(35,55), offset, line_layer_offset+1);
		drawLine(ctx, GPoint(35,55), GPoint(35,83), offset, line_layer_offset+2);
	} else if (number == 5) {
		drawLine(ctx, GPoint(22,26), GPoint(71,26), offset, line_layer_offset);
		drawLine(ctx, GPoint(0,55), GPoint(47,55), offset, line_layer_offset+1);
	} else if (number == 6) {
		drawLine(ctx, GPoint(22,26), GPoint(71,26), offset, line_layer_offset);
		drawLine(ctx, GPoint(22,55), GPoint(47,55), offset, line_layer_offset+1);
	} else if (number == 7) {
		drawLine(ctx, GPoint(0,26), GPoint(35,26), offset, line_layer_offset);
		drawLine(ctx, GPoint(35,26), GPoint(35,83), offset, line_layer_offset+1);
	} else if (number == 8) {
		drawLine(ctx, GPoint(22,26), GPoint(47,26), offset, line_layer_offset);
		drawLine(ctx, GPoint(22,55), GPoint(47,55), offset, line_layer_offset+1);
	} else if (number == 9) {
		drawLine(ctx, GPoint(22,26), GPoint(47,26), offset, line_layer_offset);
		drawLine(ctx, GPoint(0,55), GPoint(47,55), offset, line_layer_offset+1);
  } else {
		drawLine(ctx, GPoint(35,26), GPoint(35,55), offset, line_layer_offset);
	}
}

void canvas_update_callback(Layer *layer, GContext* ctx) {
	if (invertInterface) {
		graphics_context_set_fill_color(ctx, GColorWhite);
	}

	if (bluetoothConnection) {
		graphics_fill_rect(ctx, GRect(71,0,3,168), 0, GCornerNone);
		graphics_fill_rect(ctx, GRect(0,83,144,3), 0, GCornerNone);
	} else {
		graphics_fill_rect(ctx, GRect(71,0,1,168), 0, GCornerNone);
		graphics_fill_rect(ctx, GRect(73,0,1,168), 0, GCornerNone);
		graphics_fill_rect(ctx, GRect(0,83,144,1), 0, GCornerNone);
		graphics_fill_rect(ctx, GRect(0,85,144,1), 0, GCornerNone);
	}
}

void handle_bluetooth_con(bool connected) {
	bluetoothConnection = connected;
	layer_mark_dirty(canvas);
}

void topLeft_update_callback(Layer *layer, GContext* ctx) {
	renderNumber(digits[0], ctx, TOP_LEFT);
}

void topRight_update_callback(Layer *layer, GContext* ctx) {
	renderNumber(digits[1], ctx, TOP_RIGHT);
}

void bottomLeft_update_callback(Layer *layer, GContext* ctx) {
	renderNumber(digits[2], ctx, BOTTOM_LEFT);
}

void bottomRight_update_callback(Layer *layer, GContext* ctx) {
	renderNumber(digits[3], ctx, BOTTOM_RIGHT);
}

void handle_minute_tick(struct tm *tick_time, TimeUnits units_changed) {
	int hour = get_display_hour(tick_time->tm_hour);
	int minute = tick_time->tm_min;
	if (digits[1] != hour % 10) {
		digits[1] = hour % 10;
		layer_mark_dirty(digit_layers[1]);
	}
	if (digits[0] != hour / 10 % 10) {
		digits[0] = hour / 10 % 10;
		layer_mark_dirty(digit_layers[0]);
	}
	if (digits[3] != minute % 10) {
		digits[3] = minute % 10;
		layer_mark_dirty(digit_layers[3]);
	}
	if (digits[2] != minute / 10 % 10) {
		digits[2] = minute / 10 % 10;
		layer_mark_dirty(digit_layers[2]);
	}
}

void handle_init(void) {
	window = window_create();
  window_set_fullscreen(window,true);

	bluetoothIndicator = persist_exists(PERSIST_BLUETOOTH) ? persist_read_bool(PERSIST_BLUETOOTH) : false;
	invertInterface = persist_exists(PERSIST_INVERTED) ? persist_read_bool(PERSIST_INVERTED) : false;
  
	if (invertInterface) {
		window_set_background_color(window, GColorBlack);
	} else {
		window_set_background_color(window, GColorWhite);
	}

	if (bluetoothIndicator) {
		bluetooth_connection_service_subscribe(handle_bluetooth_con);
		bluetoothConnection = bluetooth_connection_service_peek();
	} else {
		bluetoothConnection = true;
	}

	struct tm *tick_time;
	time_t temp = time(NULL);
	tick_time = localtime(&temp);
	int hour = get_display_hour(tick_time->tm_hour);
	int minute = tick_time->tm_min;
  digits[1] = hour % 10;
  digits[0] = hour / 10 % 10;
  digits[3] = minute % 10;
  digits[2] = minute / 10 % 10;

	Layer *window_layer = window_get_root_layer(window);
	GRect bounds = layer_get_frame(window_layer);

	canvas = layer_create(bounds);
	layer_set_update_proc(canvas, canvas_update_callback);
	layer_add_child(window_layer, canvas);

	digit_layers[0] = layer_create(GRect(0,0,71,83));
	layer_set_update_proc(digit_layers[0], topLeft_update_callback);
	//layer_add_child(window_layer, digit_layers[0]);

	digit_layers[1] = layer_create(GRect(73,0,71,83));
	layer_set_update_proc(digit_layers[1], topRight_update_callback);
	//layer_add_child(window_layer, digit_layers[1]);

	digit_layers[2] = layer_create(GRect(0,85,71,83));
	layer_set_update_proc(digit_layers[2], bottomLeft_update_callback);
	//layer_add_child(window_layer, digit_layers[2]);

	digit_layers[3] = layer_create(GRect(73,85,71,83));
	layer_set_update_proc(digit_layers[3], bottomRight_update_callback);
	//layer_add_child(window_layer, digit_layers[3]);
  
  // create the line objects
  
  for (int i=0; i<(5*4); i++) {
    line_layers[i] = layer_create(GRect(0,0,0,0));
    layer_set_clips(line_layers[i],true);
    layer_set_update_proc(line_layers[i],drawLine_callback);
    layer_add_child(window_layer,line_layers[i]);
    line_animations[i]=NULL;
  }

	tick_timer_service_subscribe(MINUTE_UNIT, handle_minute_tick);
  window_stack_push(window, false);
  wait_timer = app_timer_register(500,wait_timer_callback,NULL);
}

void handle_deinit(void) {
	tick_timer_service_unsubscribe();
	if (bluetoothIndicator) {
		bluetooth_connection_service_unsubscribe();
	}
	window_destroy(window);
	persist_write_bool(PERSIST_INVERTED, invertInterface);
	persist_write_bool(PERSIST_BLUETOOTH, bluetoothIndicator);

}

int main(void) {
	handle_init();
	app_event_loop();
	handle_deinit();
}
