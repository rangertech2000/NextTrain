//v2.5.
#include <pebble.h>

typedef enum {
KEY_STATION1,
KEY_STATION2,
KEY_STATION3,	
KEY_TRAIN_LINE,
KEY_DEPART_TIME,
KEY_DELAY,
KEY_ARRIVE_TIME,
KEY_RECORDS,
KEY_SCHEDULE	
} AppMessageKey;
	
static char s_api_key[24];
static int s_base_app_key = 1;

static int get_app_key(AppMessageKey key) {
  return (int) key + s_base_app_key;
}

//Persisted keys
uint32_t keyStation1 = 100;
uint32_t keyStation2 = 200;

char station1_buffer[32];
char station2_buffer[32];
char *station1;
char *station2;
static char *p_departStation;
static char *p_arriveStation;
static char *p_connectStation;
int mins_left;
  
// Main window
static Window *s_main_window;
static BitmapLayer *s_background_layer;
static TextLayer *s_time_layer;
static TextLayer *title_layer;
static TextLayer *station1_layer;
static TextLayer *station2_layer;
static BitmapLayer *s_septa_logo_layer;
static TextLayer *s_date_layer;

// Train info window
static Window *s_trainInfo_window;
static Layer *s_train_countdown_canvas_layer;
static Layer *s_train_time_canvas_layer;
static BitmapLayer *s_train_bar_layer;
static TextLayer *s_train_line_layer;
static TextLayer *s_train_station1_layer;
static TextLayer *s_train_departTime_layer;
static TextLayer *s_train_time_layer;
static TextLayer *s_train_countdown_layer;
static TextLayer *s_train_countdown_label_layer;
static TextLayer *s_train_arriveTime_layer;
static TextLayer *s_train_station2_layer;
static BitmapLayer *s_train_nav_up_layer;
static BitmapLayer *s_train_nav_down_layer;

// Train schedule window
static Window *s_trainSchedule_window;
static ScrollLayer *s_trainSchedule_scroll_layer;
static TextLayer *s_trainSchedule_text_layer;
static TextLayer *s_trainSchedule_title_layer;

static GFont s_time_font_48;
static GFont s_time_font_60;

int getMinutesLeft(char *pTrainDeparts, int delay){
	// Get departure time hours and min
	static int pDepartHr = 0;
	static int pDepartMn = 0;
	char buffer[3];

	if (strlen(pTrainDeparts) == 6){
		pDepartHr = atoi(pTrainDeparts);
		pDepartMn = atoi(strncpy(buffer, pTrainDeparts+2, 2));
	}
	else if (strlen(pTrainDeparts) == 7) {
		pDepartHr = atoi(strncpy(buffer, pTrainDeparts, 2));
		pDepartMn = atoi(strncpy(buffer, pTrainDeparts+3, 2));
	}
	else {
		// Handles other departure times i.e "Cancelled"
		pDepartHr = 0;
		pDepartMn = 0;
	}
  
	// Look for 'PM'
	if (strstr(pTrainDeparts, "PM")){
		if (pDepartHr < 12) {
			pDepartHr += 12;
		}
	}
	// Future: handle times past midnight
  
	time_t timeNow;
	struct tm *departTime;
	//char bufferDepart[16];
  
	time(&timeNow);
	departTime = localtime(&timeNow);
	departTime->tm_hour = pDepartHr;
	departTime->tm_min = pDepartMn;
	int minutesUntilDeparture = ((mktime(departTime) - timeNow)/60) + delay;
 
	mins_left = minutesUntilDeparture;
	return minutesUntilDeparture;
}

static void fetchData(char *p_departStation, char *p_arriveStation, int records_to_fetch){
	// Begin dictionary
	DictionaryIterator *iter;
	app_message_outbox_begin(&iter);

	// Add a key-value pair
	dict_write_cstring(iter, get_app_key(KEY_STATION1), p_departStation);
	dict_write_cstring(iter, get_app_key(KEY_STATION2), p_arriveStation);
	dict_write_int8(iter, get_app_key(KEY_RECORDS), records_to_fetch);
	
	// Send the message!
	app_message_outbox_send();
}
  
static void inbox_received_callback(DictionaryIterator *iterator, void *context) {
	
	// Read tuples for data
	Tuple *station1_tuple = dict_find(iterator, get_app_key(KEY_STATION1));
	Tuple *station2_tuple = dict_find(iterator, get_app_key(KEY_STATION2));
	Tuple *station3_tuple = dict_find(iterator, get_app_key(KEY_STATION3));
	Tuple *depart_tuple = dict_find(iterator, get_app_key(KEY_DEPART_TIME));
	Tuple *delay_tuple = dict_find(iterator, get_app_key(KEY_DELAY));
	Tuple *arrive_tuple = dict_find(iterator, get_app_key(KEY_ARRIVE_TIME));
	Tuple *line_tuple = dict_find(iterator, get_app_key(KEY_TRAIN_LINE));
	Tuple *schedule_tuple = dict_find(iterator, get_app_key(KEY_SCHEDULE));
	
	// If station data is available, use it
	if (station1_tuple && station2_tuple) {
		printf("Station name tuples are available");
		static char station1_buffer[32];
		static char station2_buffer[32];
		
		//persist the station names
		snprintf(station1_buffer, sizeof(station1_buffer), "%s", station1_tuple->value->cstring);
		snprintf(station2_buffer, sizeof(station2_buffer), "%s", station2_tuple->value->cstring);
  		persist_write_string(keyStation1, station1_buffer);
		persist_write_string(keyStation2, station2_buffer);
		station1 = station1_buffer;
		station2 = station2_buffer;
		p_departStation = station1_buffer;
		p_arriveStation = station2_buffer;
		
		// Open the train info window
		fetchData(p_departStation, p_arriveStation, 1);
		//window_stack_push(s_trainInfo_window, false);
	} 
	else {
		printf("Station name tuples are not available");
	}
	
	// If train data is available, use it
	if (depart_tuple && delay_tuple && arrive_tuple) {
		
		static char train_line_buffer[32];
		snprintf(train_line_buffer, sizeof(train_line_buffer), "%s", line_tuple->value->cstring);
		
		// Get minutes until departure
		static char bufferMinutesLeft[4];
		int i_delay = atoi(delay_tuple->value->cstring);
		snprintf(bufferMinutesLeft, sizeof(bufferMinutesLeft), "%dm", getMinutesLeft(depart_tuple->value->cstring, i_delay));
		
		// Assemble the depart layer string
		static char depart_layer_buffer[32];
		snprintf(depart_layer_buffer, sizeof(depart_layer_buffer), "%s+%sm", depart_tuple->value->cstring, delay_tuple->value->cstring);
  		
		static char arrive_layer_buffer[12];
		snprintf(arrive_layer_buffer, sizeof(arrive_layer_buffer), "%s", arrive_tuple->value->cstring);
			 
		// Set background color
		GColor bgColor;
        
        if (i_delay == 0) {
			bgColor = GColorGreen;
		}
       	else if (i_delay > 0 && i_delay < 15) {
			bgColor = GColorYellow;
		}
		else if (i_delay > 14) {
            bgColor = GColorRed;
		}
	
		//graphics_context_set_fill_color(ctx, bgColor);
		
		// Update the text
		//window_set_background_color(s_trainInfo_window, bgColor);
		bitmap_layer_set_background_color(s_train_bar_layer, bgColor);
		text_layer_set_text(s_train_line_layer, train_line_buffer);
		text_layer_set_text(s_train_station1_layer, p_departStation);
		text_layer_set_text(s_train_departTime_layer, depart_layer_buffer);
		text_layer_set_text(s_train_countdown_layer, bufferMinutesLeft);
		text_layer_set_text(s_train_arriveTime_layer, arrive_layer_buffer);
		text_layer_set_text(s_train_station2_layer, p_arriveStation);
	}
	
	// If there is a connection
	if (station3_tuple) { 
		static char station3_buffer[32];
		snprintf(station3_buffer, sizeof(station3_buffer), "%s", station3_tuple->value->cstring);

		p_connectStation = station3_buffer;
		
		// Update the train1 info 
		text_layer_set_text(s_train_station2_layer, station3_buffer);
		bitmap_layer_set_bitmap(s_train_nav_down_layer, gbitmap_create_with_resource(RESOURCE_ID_IMAGE_TRAIN_NAV_CONNECT_1));
	
	}
	
	// If schedule is available, use it
	if (schedule_tuple) {
		static char train_line_buffer[62];
		static char schedule_buffer[512];
		
		snprintf(train_line_buffer, sizeof(train_line_buffer), 
				 "%s\nDeparts -------- Arrives", line_tuple->value->cstring);
		snprintf(schedule_buffer, sizeof(schedule_buffer), 
				 "%s", schedule_tuple->value->cstring);
		
		//Update the text
		text_layer_set_text(s_trainSchedule_title_layer, train_line_buffer);
		text_layer_set_text(s_trainSchedule_text_layer, schedule_buffer);
		
		// Trim text layer and scroll content to fit text box
		GSize max_size = text_layer_get_content_size(s_trainSchedule_text_layer);
		text_layer_set_size(s_trainSchedule_text_layer, max_size);
		scroll_layer_set_content_size(s_trainSchedule_scroll_layer, GSize(144, max_size.h + 20));
	}
}

static void inbox_dropped_callback(AppMessageResult reason, void *context) {
	APP_LOG(APP_LOG_LEVEL_ERROR, "Message dropped!");
}
static void outbox_failed_callback(DictionaryIterator *iterator, AppMessageResult reason, void *context) {
	APP_LOG(APP_LOG_LEVEL_ERROR, "Outbox send failed!");
}
static void outbox_sent_callback(DictionaryIterator *iterator, void *context) {
	APP_LOG(APP_LOG_LEVEL_INFO, "Outbox send success!");
}


static void update_time() {
	// Get a tm structure
	time_t temp = time(NULL);
	struct tm *tick_time = localtime(&temp);
  
	// Write the current hours and minutes into a buffer
	static char s_time_buffer[8];
	static char s_date_buffer[11];
	strftime(s_time_buffer, sizeof(s_time_buffer), clock_is_24h_style() ? "%H:%M" : "%I:%M", tick_time);
	strftime(s_date_buffer, sizeof(s_date_buffer), "%a %b %d", tick_time);

	// Display this time on the main window
	text_layer_set_text(s_time_layer, s_time_buffer);
	text_layer_set_text(s_date_layer, s_date_buffer);
	
	// Display this time on the train info window
	if (window_stack_contains_window(s_trainInfo_window)) {
		text_layer_set_text(s_train_time_layer, s_time_buffer);
	}	
}

static void tick_handler(struct tm *tick_time, TimeUnits units_changed) {
//static int mins_left;
static int mins_elapsed;
	// Decrement minutes left until departure
	mins_left--;

	// Update the TIME
	update_time();

	// Update if the train info window is on top
	if (window_stack_get_top_window() == s_trainInfo_window){
		// Increment the counter
		mins_elapsed++;
		
		if (mins_left > 60 && mins_elapsed > 9){
		// Only get new data every 10 minutes to save battery
			mins_elapsed = 0;
			vibes_short_pulse();
			fetchData(p_departStation, p_arriveStation, 1);
		}
		else if (mins_left > 15 && mins_elapsed > 4){
		// Only get new data every 5 minutes to save battery
			mins_elapsed = 0;
			vibes_short_pulse();
			fetchData(p_departStation, p_arriveStation, 1);
		}
		else if (mins_left <= 15){
		// Get new data every minute
			fetchData(p_departStation, p_arriveStation, 1);
		}
		else {
		// Update the countdown timer	
			static char buffer_mins_left[4];
			snprintf(buffer_mins_left, sizeof(buffer_mins_left), "%dm", mins_left);
			text_layer_set_text(s_train_countdown_layer, buffer_mins_left);
		}
	}  
}

void down_single_click_handler(ClickRecognizerRef recognizer, void *context) {
	printf("%s \n", "DOWN button clicked");

	p_departStation = station2;
	p_arriveStation = station1;

	fetchData(p_departStation, p_arriveStation, 1);

	window_stack_push(s_trainInfo_window, false);
	//Window *window = (Window *)context;
}
void up_single_click_handler(ClickRecognizerRef recognizer, void *context) {
	printf("%s \n", "UP button clicked");

	p_departStation = station1;
	p_arriveStation = station2;

	fetchData(p_departStation, p_arriveStation, 1);

	window_stack_push(s_trainInfo_window, false);
	//Window *window = (Window *)context;
}
void select_single_click_handler(ClickRecognizerRef recognizer, void *context) {
	//... called on single click, and every 1000ms of being held ...
	printf("%s \n", "SELECT button clicked");
	//Window *window = (Window *)context;
}  

void config_provider(Window *window) {
	// single click / repeat-on-hold config:
	window_single_click_subscribe(BUTTON_ID_DOWN, down_single_click_handler);
	window_single_click_subscribe(BUTTON_ID_UP, up_single_click_handler);
	window_single_repeating_click_subscribe(BUTTON_ID_SELECT, 1000, select_single_click_handler);
}

void train_up_click_handler(ClickRecognizerRef recognizer, void *context) {
	printf("p_departStation: %s", p_departStation);
	printf("p_connectStation: %s", p_connectStation);
	printf("p_arriveStation: %s", p_arriveStation);

	if (p_departStation == station1 || p_departStation == station2) {
		fetchData(p_departStation, p_arriveStation, 20);
		
		window_stack_push(s_trainSchedule_window, false);
	}
	else {
		p_departStation = station1;
		p_arriveStation = p_connectStation;
		bitmap_layer_set_bitmap(s_train_nav_up_layer, gbitmap_create_with_resource(RESOURCE_ID_IMAGE_TRAIN_NAV_SCHEDULE));
		bitmap_layer_set_bitmap(s_train_nav_down_layer, gbitmap_create_with_resource(RESOURCE_ID_IMAGE_TRAIN_NAV_CONNECT_1));
		
		fetchData(p_departStation, p_arriveStation, 1);
	}
}
void train_select_click_handler(ClickRecognizerRef recognizer, void *context) {
	printf("%s \n", "SELECT button clicked");
	vibes_short_pulse();
	fetchData(p_departStation, p_arriveStation, 1);
} 
void train_down_click_handler(ClickRecognizerRef recognizer, void *context) {
	printf("p_departStation: %s", p_departStation);
	printf("p_connectStation: %s", p_connectStation);
	printf("p_arriveStation: %s", p_arriveStation);
	
	if (p_connectStation){
		if (p_departStation == station1 || p_departStation == station2) {
			p_departStation = p_connectStation;
			p_arriveStation = station2;
			bitmap_layer_set_bitmap(s_train_nav_up_layer, gbitmap_create_with_resource(RESOURCE_ID_IMAGE_TRAIN_NAV_CONNECT_2));
			bitmap_layer_set_bitmap(s_train_nav_down_layer, gbitmap_create_with_resource(RESOURCE_ID_IMAGE_TRAIN_NAV_SCHEDULE));
			
			fetchData(p_departStation, p_arriveStation, 1);
		}
		else{
			fetchData(p_connectStation, p_arriveStation, 20);
			
			window_stack_push(s_trainSchedule_window, false);
		}
	}
}
void train_window_config_provider(Window *window) {
	window_single_click_subscribe(BUTTON_ID_UP, train_up_click_handler);
	window_single_click_subscribe(BUTTON_ID_SELECT, train_select_click_handler);
	window_single_click_subscribe(BUTTON_ID_DOWN, train_down_click_handler);
} 


static void main_window_load(Window *window) {
	// Get information about the Window
	Layer *window_layer = window_get_root_layer(window);
	GRect bounds = layer_get_bounds(window_layer);

	// Create background image layer
	s_background_layer = bitmap_layer_create(
	  GRect(0, 0, 144, 168));
	bitmap_layer_set_bitmap(s_background_layer, gbitmap_create_with_resource(RESOURCE_ID_IMAGE_BACKGROUND));
	bitmap_layer_set_background_color(s_background_layer, GColorClear);
	bitmap_layer_set_compositing_mode(s_background_layer, GCompOpSet);
	
	// Create Septa logo layer
	s_septa_logo_layer = bitmap_layer_create(
	  GRect(PBL_IF_ROUND_ELSE(35, 5), PBL_IF_ROUND_ELSE(10, 8), 36, 27));
	bitmap_layer_set_bitmap(s_septa_logo_layer, gbitmap_create_with_resource(RESOURCE_ID_IMAGE_SEPTA_LOGO));
	bitmap_layer_set_background_color(s_septa_logo_layer, GColorClear);
	
	title_layer = text_layer_create(GRect(46, 5, 80, 28));
	text_layer_set_background_color(title_layer, GColorClear);
	text_layer_set_text_color(title_layer, GColorBlack);
	text_layer_set_text(title_layer, "Next\nTrain");
	
	// Create GFont
	s_time_font_48 = fonts_load_custom_font(resource_get_handle(RESOURCE_ID_FONT_WHITE_RABBIT_48));  
	// Create the time layer
	s_time_layer = text_layer_create(
	GRect(0, PBL_IF_ROUND_ELSE(58, 52), bounds.size.w, 50));
	text_layer_set_background_color(s_time_layer, GColorClear);
	text_layer_set_text_alignment(s_time_layer, GTextAlignmentCenter);
	text_layer_set_text_color(s_time_layer, GColorBlack);
	text_layer_set_text(s_time_layer, "00:00");
	text_layer_set_font(s_time_layer, s_time_font_48);

	station1_layer = text_layer_create(GRect(0, 46, 144, 20));
	text_layer_set_background_color(station1_layer, GColorClear);
	text_layer_set_text_alignment(station1_layer, GTextAlignmentCenter);
	text_layer_set_text_color(station1_layer, GColorWhite);
	text_layer_set_text(station1_layer, station1);
	
	station2_layer = text_layer_create(GRect(0, 103, 144, 20));
	text_layer_set_background_color(station2_layer, GColorClear);
	text_layer_set_text_alignment(station2_layer, GTextAlignmentCenter);
	text_layer_set_text_color(station2_layer, GColorWhite);
	text_layer_set_text(station2_layer, station2);
	
	// Date layer
  	s_date_layer = text_layer_create(GRect(4, 144, 100, 20));
	text_layer_set_font(s_date_layer, fonts_get_system_font(FONT_KEY_GOTHIC_18_BOLD));
 	text_layer_set_background_color(s_date_layer, GColorClear);
	text_layer_set_text_alignment(s_date_layer, GTextAlignmentLeft);
	text_layer_set_text_color(s_date_layer, GColorBlack);
  	//text_layer_set_text(s_date_layer, "Date");
  	

	// Add it as a child layer to the Window's root layer
	layer_add_child(window_layer, bitmap_layer_get_layer(s_background_layer));
	layer_add_child(window_layer, text_layer_get_layer(s_time_layer));
	layer_add_child(window_layer, text_layer_get_layer(title_layer));
	layer_add_child(window_layer, text_layer_get_layer(station1_layer));
	layer_add_child(window_layer, text_layer_get_layer(station2_layer));
	layer_add_child(window_layer, bitmap_layer_get_layer(s_septa_logo_layer));
	layer_add_child(window_layer, (Layer *)s_date_layer);
}

static void main_window_unload(Window *window) {
	bitmap_layer_destroy(s_background_layer);
	text_layer_destroy(s_time_layer);
	text_layer_destroy(title_layer);
	text_layer_destroy(station1_layer);
	text_layer_destroy(station2_layer);
	fonts_unload_custom_font(s_time_font_48);
}

static void update_proc_train_countdown(Layer *layer, GContext *ctx) {
	//GRect bounds = layer_get_bounds(layer);
	GRect bounds = GRect(19, 35, 104, 81);
	graphics_context_set_stroke_width(ctx, 2);
	graphics_context_set_stroke_color(ctx, GColorBlue);
	graphics_draw_round_rect(ctx, bounds, 4);
}

static void update_proc_train_time(Layer *layer, GContext *ctx) {
	GRect bounds = GRect(88, 56, 60, 24);
	graphics_context_set_fill_color(ctx, GColorBlue);
	graphics_fill_rect(ctx, bounds, 4, GCornersLeft);
}

static void trainInfo_window_load(Window *trainInfo_window) {
	// Get information about the Window
	Layer *window_layer = window_get_root_layer(trainInfo_window);
	GRect bounds = layer_get_bounds(window_layer);
	window_set_background_color(trainInfo_window, GColorBlack);
	
	// Create countdown canvas layer
	s_train_countdown_canvas_layer = layer_create(bounds);
	layer_set_update_proc(s_train_countdown_canvas_layer, update_proc_train_countdown);
	layer_add_child(window_layer, s_train_countdown_canvas_layer);

	// Create train line layer
	s_train_line_layer = text_layer_create(
	  GRect(PBL_IF_ROUND_ELSE(30, 0), PBL_IF_ROUND_ELSE(5, -4), (bounds.size.w), 18));
	text_layer_set_background_color(s_train_line_layer, GColorClear);
	text_layer_set_font(s_train_line_layer, fonts_get_system_font(FONT_KEY_GOTHIC_14));
	text_layer_set_text_color(s_train_line_layer, GColorWhite);
	text_layer_set_text_alignment(s_train_line_layer, GTextAlignmentCenter);
	text_layer_set_overflow_mode(s_train_line_layer, GTextOverflowModeWordWrap);
	//text_layer_set_text(s_train_line_layer, station1);
	layer_add_child(window_get_root_layer(trainInfo_window), text_layer_get_layer(s_train_line_layer));
	
	// Create train bar layer
	s_train_bar_layer = bitmap_layer_create(
	  GRect(PBL_IF_ROUND_ELSE(30, 0), PBL_IF_ROUND_ELSE(43, 34), 16, 118));
	bitmap_layer_set_bitmap(s_train_bar_layer, gbitmap_create_with_resource(RESOURCE_ID_IMAGE_TRAIN_BAR));
	//bitmap_layer_set_background_color(s_train_bar_layer, GColorClear);
	bitmap_layer_set_compositing_mode(s_train_bar_layer, GCompOpSet);
	layer_add_child(window_get_root_layer(trainInfo_window), bitmap_layer_get_layer(s_train_bar_layer));

	// Create depart station layer
	s_train_station1_layer = text_layer_create(
	  GRect(PBL_IF_ROUND_ELSE(30, 0), PBL_IF_ROUND_ELSE(21, 9), (bounds.size.w), 21));
	text_layer_set_background_color(s_train_station1_layer, GColorClear);
	text_layer_set_font(s_train_station1_layer, fonts_get_system_font(FONT_KEY_GOTHIC_18_BOLD));
	text_layer_set_text_color(s_train_station1_layer, GColorWhite);
	text_layer_set_text_alignment(s_train_station1_layer, GTextAlignmentLeft);
	text_layer_set_overflow_mode(s_train_station1_layer, GTextOverflowModeWordWrap);
	//text_layer_set_text(s_train_station1_layer, station1);
	layer_add_child(window_get_root_layer(trainInfo_window), text_layer_get_layer(s_train_station1_layer));
  
	// Create depart time layer
	s_train_departTime_layer = text_layer_create(
	  GRect(PBL_IF_ROUND_ELSE(49, 19), 29, (bounds.size.w - 19), 24));
	text_layer_set_background_color(s_train_departTime_layer, GColorClear);
	text_layer_set_font(s_train_departTime_layer, fonts_get_system_font(FONT_KEY_GOTHIC_24_BOLD));
	text_layer_set_text_color(s_train_departTime_layer, GColorWhite);
	text_layer_set_text_alignment(s_train_departTime_layer, GTextAlignmentLeft);
	text_layer_set_overflow_mode(s_train_departTime_layer, GTextOverflowModeWordWrap);
	text_layer_set_text(s_train_departTime_layer, "Loading...");
	layer_add_child(window_get_root_layer(trainInfo_window), text_layer_get_layer(s_train_departTime_layer));
	
	// Create countdown label layer
	s_train_countdown_label_layer = text_layer_create(
	GRect(PBL_IF_ROUND_ELSE(49, 22), 52, (bounds.size.w - 19), 16));
	text_layer_set_background_color(s_train_countdown_label_layer, GColorClear);
	text_layer_set_text_color(s_train_countdown_label_layer, GColorWhite);
	text_layer_set_text_alignment(s_train_countdown_label_layer, GTextAlignmentLeft);
	text_layer_set_font(s_train_countdown_label_layer, fonts_get_system_font(FONT_KEY_GOTHIC_14));
	text_layer_set_text(s_train_countdown_label_layer, "Leaving in:");
	layer_add_child(window_get_root_layer(trainInfo_window), text_layer_get_layer(s_train_countdown_label_layer));
	
	// Create GFont
	s_time_font_60 = fonts_load_custom_font(resource_get_handle(RESOURCE_ID_FONT_WHITE_RABBIT_60));  
	// Create countdown layer
	s_train_countdown_layer = text_layer_create(
	GRect(PBL_IF_ROUND_ELSE(49, 20), 52, (bounds.size.w - 19), 60));
	text_layer_set_background_color(s_train_countdown_layer, GColorClear);
	text_layer_set_text_color(s_train_countdown_layer, GColorWhite);
	text_layer_set_text_alignment(s_train_countdown_layer, GTextAlignmentLeft);
	//text_layer_set_font(s_train_countdown_layer, fonts_get_system_font(FONT_KEY_BITHAM_42_MEDIUM_NUMBERS));
	text_layer_set_font(s_train_countdown_layer, s_time_font_60);
	text_layer_set_text(s_train_countdown_layer, "00");
	layer_add_child(window_get_root_layer(trainInfo_window), text_layer_get_layer(s_train_countdown_layer));
			
	// Create time canvas layer
	s_train_time_canvas_layer = layer_create(bounds);
	layer_set_update_proc(s_train_time_canvas_layer, update_proc_train_time);
	layer_add_child(window_layer, s_train_time_canvas_layer);
	
	// Create the time text layer
	s_train_time_layer = text_layer_create(GRect(90, PBL_IF_ROUND_ELSE(69, 49), 54, 28));
	text_layer_set_background_color(s_train_time_layer, GColorClear);
	text_layer_set_text_alignment(s_train_time_layer, GTextAlignmentRight);
	text_layer_set_text_color(s_train_time_layer, GColorWhite);
	text_layer_set_text(s_train_time_layer, "00:00");
	text_layer_set_font(s_train_time_layer, fonts_get_system_font(FONT_KEY_GOTHIC_28));
	layer_add_child(window_get_root_layer(trainInfo_window), text_layer_get_layer(s_train_time_layer));
	
	// Create arrive time Layer
	s_train_arriveTime_layer = text_layer_create(
	  GRect(PBL_IF_ROUND_ELSE(49, 19), 126, (bounds.size.w - 19), 24));
	text_layer_set_background_color(s_train_arriveTime_layer, GColorClear);
	text_layer_set_font(s_train_arriveTime_layer, fonts_get_system_font(FONT_KEY_GOTHIC_24));
	text_layer_set_text_color(s_train_arriveTime_layer, GColorWhite);
	text_layer_set_text_alignment(s_train_arriveTime_layer, GTextAlignmentLeft);
	text_layer_set_overflow_mode(s_train_arriveTime_layer, GTextOverflowModeWordWrap);
	text_layer_set_text(s_train_arriveTime_layer, "Loading...");
	layer_add_child(window_get_root_layer(trainInfo_window), text_layer_get_layer(s_train_arriveTime_layer));
  
	// Create arrive station layer
	s_train_station2_layer = text_layer_create(
	  GRect(PBL_IF_ROUND_ELSE(30, 0), 148, (bounds.size.w), 20));
	text_layer_set_background_color(s_train_station2_layer, GColorClear);
	text_layer_set_font(s_train_station2_layer, fonts_get_system_font(FONT_KEY_GOTHIC_18));
	text_layer_set_text_color(s_train_station2_layer, GColorWhite);
	text_layer_set_text_alignment(s_train_station2_layer, GTextAlignmentLeft);
	text_layer_set_overflow_mode(s_train_station2_layer, GTextOverflowModeWordWrap);
	//text_layer_set_text(s_train_station1_layer, station2);
	layer_add_child(window_get_root_layer(trainInfo_window), text_layer_get_layer(s_train_station2_layer));

	// Create train navigation schedule layer
	s_train_nav_up_layer = bitmap_layer_create(
	  GRect(PBL_IF_ROUND_ELSE(121, 126), PBL_IF_ROUND_ELSE(25, 16), 18, 42));
	bitmap_layer_set_bitmap(s_train_nav_up_layer, gbitmap_create_with_resource(RESOURCE_ID_IMAGE_TRAIN_NAV_SCHEDULE));
	//bitmap_layer_set_background_color(s_train_nav_up_layer, GColorClear);
	bitmap_layer_set_compositing_mode(s_train_nav_up_layer, GCompOpSet);
	layer_add_child(window_get_root_layer(trainInfo_window), bitmap_layer_get_layer(s_train_nav_up_layer));
	
	// Create train navigation connect layer
	s_train_nav_down_layer = bitmap_layer_create(
	  GRect(PBL_IF_ROUND_ELSE(123, 126), PBL_IF_ROUND_ELSE(109, 114), 18, 42));
	//bitmap_layer_set_bitmap(s_train_nav_down_layer, gbitmap_create_with_resource(RESOURCE_ID_IMAGE_TRAIN_NAV_CONNECT));
	//bitmap_layer_set_background_color(s_train_nav_down_layer, GColorClear);
	bitmap_layer_set_compositing_mode(s_train_nav_down_layer, GCompOpSet);
	layer_add_child(window_get_root_layer(trainInfo_window), bitmap_layer_get_layer(s_train_nav_down_layer));
	
	// Update the TIME
	update_time();
	
	// Display the window
	window_stack_push(s_trainInfo_window, true);
	
	// Set click providers 
	window_set_click_config_provider(s_trainInfo_window, (ClickConfigProvider) train_window_config_provider);
}

static void trainInfo_window_unload(Window *trainInfo_window) {
	// Destroy train window elements
	layer_destroy(s_train_countdown_canvas_layer);
	layer_destroy(s_train_time_canvas_layer);
	text_layer_destroy(s_train_line_layer);
	bitmap_layer_destroy(s_train_bar_layer);
	text_layer_destroy(s_train_station1_layer);
	text_layer_destroy(s_train_departTime_layer);
	text_layer_destroy(s_train_time_layer);
	text_layer_destroy(s_train_countdown_label_layer);
	fonts_unload_custom_font(s_time_font_60);
	text_layer_destroy(s_train_countdown_layer);
	text_layer_destroy(s_train_arriveTime_layer);
	text_layer_destroy(s_train_station2_layer);
	bitmap_layer_destroy(s_train_nav_up_layer);
	bitmap_layer_destroy(s_train_nav_down_layer);
}


static void trainSchedule_window_load(Window *trainSchedule_window) {
	// Get information about the Window
	Layer *window_layer = window_get_root_layer(trainSchedule_window);
	GRect bounds = layer_get_frame(window_layer);
	GRect max_text_bounds = GRect(0, 28, bounds.size.w, 2000);
	window_set_background_color(trainSchedule_window, GColorBlack);

	// Create the scroll layer
	s_trainSchedule_scroll_layer = scroll_layer_create(bounds);
	scroll_layer_set_click_config_onto_window(s_trainSchedule_scroll_layer, trainSchedule_window);
/*	
	// Scrolls through text on a round screen by page (scroll offset is changed by frame's height)
	#ifdef PBL_ROUND
		scroll_layer_set_paging(s_trainSchedule_scroll_layer, true);
	#endif
*/	
	// Create title layer
	s_trainSchedule_title_layer = text_layer_create(GRect(0,-2,144,30));
	text_layer_set_background_color(s_trainSchedule_title_layer, GColorBlack);
	text_layer_set_text_color(s_trainSchedule_title_layer, GColorWhite);
	text_layer_set_text_alignment(s_trainSchedule_title_layer, GTextAlignmentCenter);
	text_layer_set_font(s_trainSchedule_title_layer, fonts_get_system_font(FONT_KEY_GOTHIC_14));
	//text_layer_set_text(s_trainSchedule_title_layer, "Manayunk/Norristown Line\nDeparts -------- Arrives");
	
	// Create text layer
	s_trainSchedule_text_layer = text_layer_create(max_text_bounds);
	text_layer_set_background_color(s_trainSchedule_text_layer, GColorBlack);
	text_layer_set_text_color(s_trainSchedule_text_layer, GColorWhite);
	text_layer_set_text(s_trainSchedule_text_layer, "Loading Schedule...");
	text_layer_set_font(s_trainSchedule_text_layer, fonts_get_system_font(FONT_KEY_GOTHIC_24));
	
	// Add the layers for display
	scroll_layer_add_child(s_trainSchedule_scroll_layer, text_layer_get_layer(s_trainSchedule_title_layer));
	scroll_layer_add_child(s_trainSchedule_scroll_layer, text_layer_get_layer(s_trainSchedule_text_layer));
	layer_add_child(window_get_root_layer(trainSchedule_window), scroll_layer_get_layer(s_trainSchedule_scroll_layer));
/*	
	// Fit the text to the round screen with inset around edge of screen
	#ifdef PBL_ROUND
		text_layer_set_text_alignment(s_trainSchedule_text_layer, GTextAlignmentCenter);
		uint8_t inset = 4;
		text_layer_enable_screen_text_flow_and_paging(s_trainSchedule_text_layer, inset);
	#endif
*/	
/*	
	// Trim text layer and scroll content to fit text box
	GSize max_size = text_layer_get_content_size(s_trainSchedule_text_layer);
	text_layer_set_size(s_trainSchedule_text_layer, max_size);
	scroll_layer_set_content_size(s_trainSchedule_scroll_layer,  GSize(bounds.size.w, max_size.h + 4));
*/
 /* 
	text_layer_set_background_color(s_trainSchedule_text_layer, GColorClear);
	text_layer_set_text_color(s_trainSchedule_text_layer, GColorWhite);
	text_layer_set_text_alignment(s_trainSchedule_text_layer, GTextAlignmentLeft);
	text_layer_set_overflow_mode(s_trainSchedule_text_layer, GTextOverflowModeWordWrap);
*/	

	// Display the window
	window_stack_push(s_trainSchedule_window, true);
}

static void trainSchedule_window_unload(Window *trainSchedule_window) {
	// Destroy train schedule window elements
	scroll_layer_destroy(s_trainSchedule_scroll_layer);
	text_layer_destroy(s_trainSchedule_text_layer);
	text_layer_destroy(s_trainSchedule_title_layer);
}


static void init() { 
	//Check for persisted keys
	if (persist_get_size(keyStation1) > 1) {
  		// Read persisted value
  		persist_read_string(keyStation1, station1_buffer, sizeof(station1_buffer));
		printf("Persisted station1: %s", station1_buffer);
		station1 = station1_buffer;
	} 
	else {
  		// Choose a default value
  		station1 = "Wissahickon";
		printf("Assigned station1: %s", station1);

  		// Remember the default value until the user chooses their own value
  		persist_write_string(keyStation1, station1_buffer);
	}
	
	if (persist_get_size(keyStation2) > 1) {
  		// Read persisted value
  		persist_read_string(keyStation2, station2_buffer, sizeof(station2_buffer));
		printf("Persisted station2: %s", station2_buffer);
		station2 = station2_buffer;
	} 
	else {
  		// Choose a default value
  		station2 = "Penllyn";
		printf("Assigned station2: %s", station2);
		
  		// Remember the default value until the user chooses their own value
  		persist_write_string(keyStation2, station2_buffer);
	}
	
	// Create main Window element and assign to pointer
	s_main_window = window_create();
	window_set_background_color(s_main_window, GColorWhite);

	// Set handlers to manage the elements inside the Window
	window_set_window_handlers(s_main_window, (WindowHandlers) {
		.load = main_window_load,
		.unload = main_window_unload
	});

	// Show the Window on the watch, with animated=true
	window_stack_push(s_main_window, false);

	// Set click providers 
	window_set_click_config_provider(s_main_window, (ClickConfigProvider) config_provider);
	
	// Register callbacks
	app_message_register_inbox_received(inbox_received_callback);
	app_message_register_inbox_dropped(inbox_dropped_callback);
	app_message_register_outbox_failed(outbox_failed_callback);
	app_message_register_outbox_sent(outbox_sent_callback);

	// Open AppMessage
	//app_message_open(app_message_inbox_size_maximum(), app_message_outbox_size_maximum());
	app_message_open(512, 64);
	
	// Create the train info window 
	s_trainInfo_window = window_create();
	window_set_window_handlers(s_trainInfo_window, (WindowHandlers) {
		.load = trainInfo_window_load,
		.unload = trainInfo_window_unload
	});
	
	
	// Create the train schedule window 
	s_trainSchedule_window = window_create();
	window_set_window_handlers(s_trainSchedule_window, (WindowHandlers) {
		.load = trainSchedule_window_load,
		.unload = trainSchedule_window_unload
	});
	
	// Register with TickTimerService
	tick_timer_service_subscribe(MINUTE_UNIT, tick_handler);
	// Update the time
	update_time();
}

static void deinit() {
	// Destroy Window
	window_destroy(s_main_window);
	window_destroy(s_trainInfo_window);
	window_destroy(s_trainSchedule_window);
}

int main(void) {
	init();
	app_event_loop();
	deinit();
}