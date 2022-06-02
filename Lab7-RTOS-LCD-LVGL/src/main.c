/************************************************************************/
/* includes                                                             */
/************************************************************************/

#include <asf.h>
#include <string.h>
#include "ili9341.h"
#include "lvgl.h"
#include "touch/touch.h"

LV_FONT_DECLARE(dseg70);
LV_FONT_DECLARE(dseg60);
LV_FONT_DECLARE(dseg50);
LV_FONT_DECLARE(dseg15);

typedef struct  {
	uint32_t year;
	uint32_t month;
	uint32_t day;
	uint32_t week;
	uint32_t hour;
	uint32_t minute;
	uint32_t second;
} calendar;

volatile char flag_rtc_second = 0;
volatile char counter = 0;
volatile char clk_config = 0;
volatile char power = 1;

volatile 	uint32_t current_hour, current_min, current_sec;
volatile uint32_t current_year, current_month, current_day, current_week;

QueueHandle_t xQueueClk;

// global
lv_obj_t * labelBtn1;
lv_obj_t * labelBtnMenu;
lv_obj_t * labelBtnClk;
lv_obj_t * labelBtnUp;
lv_obj_t * labelBtnDown;

/************************************************************************/
/* LCD / LVGL                                                           */
/************************************************************************/

#define LV_HOR_RES_MAX          (320)
#define LV_VER_RES_MAX          (240)

/*A static or global variable to store the buffers*/
static lv_disp_draw_buf_t disp_buf;

/*Static or global buffer(s). The second buffer is optional*/
static lv_color_t buf_1[LV_HOR_RES_MAX * LV_VER_RES_MAX];
static lv_disp_drv_t disp_drv;          /*A variable to hold the drivers. Must be static or global.*/
static lv_indev_drv_t indev_drv;

/************************************************************************/
/* RTOS                                                                 */
/************************************************************************/

#define TASK_LCD_STACK_SIZE                (1024*6/sizeof(portSTACK_TYPE))
#define TASK_LCD_STACK_PRIORITY            (tskIDLE_PRIORITY)

#define TASK_RTC_STACK_SIZE                (1024*6/sizeof(portSTACK_TYPE))
#define TASK_RTC_STACK_PRIORITY            (tskIDLE_PRIORITY)

extern void vApplicationStackOverflowHook(xTaskHandle *pxTask,  signed char *pcTaskName);
extern void vApplicationIdleHook(void);
extern void vApplicationTickHook(void);
extern void vApplicationMallocFailedHook(void);
extern void xPortSysTickHandler(void);

extern void vApplicationStackOverflowHook(xTaskHandle *pxTask, signed char *pcTaskName) {
	printf("stack overflow %x %s\r\n", pxTask, (portCHAR *)pcTaskName);
	for (;;) {	}
}

extern void vApplicationIdleHook(void) { }

extern void vApplicationTickHook(void) { }

extern void vApplicationMallocFailedHook(void) {
	configASSERT( ( volatile void * ) NULL );
}

void RTC_init(Rtc *rtc, uint32_t id_rtc, calendar t, uint32_t irq_type);

void RTC_Handler(void) {
	uint32_t ul_status = rtc_get_status(RTC);
	
	/* second tick */
	if ((ul_status & RTC_SR_SEC) == RTC_SR_SEC) {
		flag_rtc_second = 1;
		BaseType_t xHigherPriorityTaskWoken = pdFALSE;
		xQueueSendFromISR(xQueueClk, &flag_rtc_second, &xHigherPriorityTaskWoken);
	}
	
	/* Time or date alarm */
	if ((ul_status & RTC_SR_ALARM) == RTC_SR_ALARM) {
	}

	rtc_clear_status(RTC, RTC_SCCR_SECCLR);
	rtc_clear_status(RTC, RTC_SCCR_ALRCLR);
	rtc_clear_status(RTC, RTC_SCCR_ACKCLR);
	rtc_clear_status(RTC, RTC_SCCR_TIMCLR);
	rtc_clear_status(RTC, RTC_SCCR_CALCLR);
	rtc_clear_status(RTC, RTC_SCCR_TDERRCLR);
}

/************************************************************************/
/* lvgl                                                                 */
/************************************************************************/

lv_obj_t * labelFloor;
lv_obj_t * labelDigit;
lv_obj_t * labelSetValue;
lv_obj_t * labelClock;
lv_obj_t * labelTemp;
lv_obj_t * labelTempSet;

static void event_handler(lv_event_t * e) {
	lv_event_code_t code = lv_event_get_code(e);

	if(code == LV_EVENT_CLICKED) {
		printf("Clicked");
	}
	else if(code == LV_EVENT_VALUE_CHANGED) {
		printf("Toggled");
	}
}

static void menu_handler(lv_event_t * e) {
	lv_event_code_t code = lv_event_get_code(e);

	if(code == LV_EVENT_CLICKED) {
		LV_LOG_USER("Clicked");
	}
	else if(code == LV_EVENT_VALUE_CHANGED) {
		LV_LOG_USER("Toggled");
	}
}

static void clk_handler(lv_event_t * e) {
	lv_event_code_t code = lv_event_get_code(e);

	if(code == LV_EVENT_CLICKED) {
		LV_LOG_USER("Clicked");
		
		if (clk_config == 0){
			clk_config = 1;
			} else {
			clk_config = 0;
		}
	}
	else if(code == LV_EVENT_VALUE_CHANGED) {
		LV_LOG_USER("Toggled");
	}
}

static void down_handler(lv_event_t * e) {
	lv_event_code_t code = lv_event_get_code(e);
	char *c;
	int temp;
	
	if(code == LV_EVENT_CLICKED) {
		if(clk_config == 1){
			current_min = current_min - 1;
			
			if (current_min == -1 ){
				current_hour = current_hour - 1;
				current_min = 59;
				
				if (current_hour == -1){
					current_hour = 23;
				}
			}
			
			rtc_set_time(RTC, current_hour, current_min, current_sec);
			
			} else{
			c = lv_label_get_text(labelSetValue);
			temp = atoi(c);
			lv_label_set_text_fmt(labelSetValue, "%02d", temp - 1);
		}
	}
}

static void up_handler(lv_event_t * e) {
	lv_event_code_t code = lv_event_get_code(e);
	char *c;
	int temp;
	if(code == LV_EVENT_CLICKED) {
		if(clk_config == 1){
			current_min = current_min + 1;
			
			if (current_min == 60){
				current_hour = current_hour + 1;
				current_min = 0;
				
				if (current_hour == 24){
					current_hour = 0;
				}
			}
			
			rtc_set_time(RTC, current_hour, current_min, current_sec);
			
			} else{
			c = lv_label_get_text(labelSetValue);
			temp = atoi(c);
			lv_label_set_text_fmt(labelSetValue, "%02d", temp + 1);
		}
	}
}

void lv_ex_btn_1(void) {
	lv_obj_t * label;

	lv_obj_t * btn1 = lv_btn_create(lv_scr_act());
	lv_obj_add_event_cb(btn1, event_handler, LV_EVENT_ALL, NULL);
	lv_obj_align(btn1, LV_ALIGN_CENTER, 0, -40);

	label = lv_label_create(btn1);
	lv_label_set_text(label, "Corsi");
	lv_obj_center(label);

	lv_obj_t * btn2 = lv_btn_create(lv_scr_act());
	lv_obj_add_event_cb(btn2, event_handler, LV_EVENT_ALL, NULL);
	lv_obj_align(btn2, LV_ALIGN_CENTER, 0, 40);
	lv_obj_add_flag(btn2, LV_OBJ_FLAG_CHECKABLE);
	lv_obj_set_height(btn2, LV_SIZE_CONTENT);

	label = lv_label_create(btn2);
	lv_label_set_text(label, "Toggle");
	lv_obj_center(label);
}

void lv_termostato(void) {
		
		static lv_style_t style;
		lv_style_init(&style);
		lv_style_set_bg_color(&style, lv_color_black());
		lv_style_set_border_color(&style, lv_palette_main(LV_PALETTE_GREEN));
		lv_style_set_border_width(&style, 1);
		
		//btn1
	    lv_obj_t * btn1 = lv_btn_create(lv_scr_act());
	    lv_obj_add_event_cb(btn1, event_handler, LV_EVENT_ALL, NULL);
	    lv_obj_align(btn1, LV_ALIGN_BOTTOM_LEFT, 10, -20);
		lv_obj_add_style(btn1, &style, 0);
		
		lv_obj_set_width(btn1, 55);  
		lv_obj_set_height(btn1, 55);

	    labelBtn1 = lv_label_create(btn1);
	    lv_label_set_text(labelBtn1, "[ " LV_SYMBOL_POWER);
	    lv_obj_center(labelBtn1);
		
// 		//btnMenu
		lv_obj_t * btnMenu = lv_btn_create(lv_scr_act());
	    lv_obj_add_event_cb(btnMenu, menu_handler, LV_EVENT_ALL, NULL);
	    lv_obj_align_to(btnMenu, btn1, LV_ALIGN_OUT_RIGHT_MID, 0, -19);
		lv_obj_add_style(btnMenu, &style, 0);
// 		
		lv_obj_set_width(btnMenu, 55);  
		lv_obj_set_height(btnMenu, 55);
// 
	    labelBtnMenu = lv_label_create(btnMenu);
	    lv_label_set_text(labelBtnMenu, "| M |" );
	    lv_obj_center(labelBtnMenu);
		
		//btnClk
		lv_obj_t * btnClk = lv_btn_create(lv_scr_act());
		lv_obj_add_event_cb(btn1, clk_handler, LV_EVENT_ALL, NULL);
		lv_obj_align_to( btnClk, btnMenu, LV_ALIGN_OUT_RIGHT_MID, 0, -19);
		lv_obj_add_style(btnClk, &style, 0);
		
		lv_obj_set_width(btnClk, 55);
		lv_obj_set_height(btnClk, 55);

		labelBtnClk = lv_label_create(btnClk);
		lv_label_set_text(labelBtnClk, LV_SYMBOL_REFRESH " ]" );
		lv_obj_center(labelBtnClk);
		
		//btnDown
		lv_obj_t * btnDown = lv_btn_create(lv_scr_act());
		lv_obj_add_event_cb(btnDown, down_handler, LV_EVENT_ALL, NULL);
		lv_obj_align_to( btnDown, btnClk, LV_ALIGN_OUT_RIGHT_MID, 40, -19);
		lv_obj_add_style(btnDown, &style, 0);
		
		lv_obj_set_width(btnDown, 55);
		lv_obj_set_height(btnDown, 55);

		labelBtnDown = lv_label_create(btnDown);
		lv_label_set_text(labelBtnDown, "[ " LV_SYMBOL_DOWN);
		lv_obj_center(labelBtnDown);
		
		//btnUp
		lv_obj_t * btnUp = lv_btn_create(lv_scr_act());
		lv_obj_add_event_cb(btnUp, up_handler, LV_EVENT_ALL, NULL);
		lv_obj_align_to( btnUp, btnDown, LV_ALIGN_OUT_RIGHT_MID, 0, -19);
		lv_obj_add_style(btnUp, &style, 0);
		
		lv_obj_set_width(btnUp, 55);
		lv_obj_set_height(btnUp, 55);

		labelBtnUp = lv_label_create(btnUp);
		lv_label_set_text(labelBtnUp, LV_SYMBOL_UP " ]");
		lv_obj_center(labelBtnUp);
		
		
		labelFloor = lv_label_create(lv_scr_act());
		lv_obj_align(labelFloor, LV_ALIGN_LEFT_MID, 35 , -45);
		lv_obj_set_style_text_font(labelFloor, &dseg70, LV_STATE_DEFAULT);
		lv_obj_set_style_text_color(labelFloor, lv_color_white(), LV_STATE_DEFAULT);
		lv_label_set_text_fmt(labelFloor, "%02d", 23);
		
// 		labelDigit = lv_label_create(lv_scr_act());
// 		lv_obj_align_to(labelDigit, LV_ALIGN_BOTTOM_RIGHT, 0, 0, 0);
// 		lv_obj_set_style_text_font(labelDigit, &dseg15, LV_STATE_DEFAULT);
// 		lv_obj_set_style_text_color(labelDigit, lv_color_white(), LV_STATE_DEFAULT);
// 		lv_label_set_text_fmt(labelDigit, ".%1d", 0);

		labelSetValue = lv_label_create(lv_scr_act());
		lv_obj_align(labelSetValue, LV_ALIGN_OUT_RIGHT_MID, 200 , 50);
		lv_obj_set_style_text_font(labelSetValue, &dseg60, LV_STATE_DEFAULT);
		lv_obj_set_style_text_color(labelSetValue, lv_color_white(), LV_STATE_DEFAULT);
		lv_label_set_text_fmt(labelSetValue, "%02d", 22);
		
		labelTempSet = lv_label_create(lv_scr_act());
		lv_obj_align(labelTempSet, LV_ALIGN_OUT_RIGHT_MID, 200 , 75);
		lv_obj_set_style_text_font(labelTempSet, &dseg15, LV_STATE_DEFAULT);
		lv_obj_set_style_text_color(labelTempSet, lv_color_white(), LV_STATE_DEFAULT);
		lv_label_set_text_fmt(labelTempSet, "Temp Atual");
		
		labelClock = lv_label_create(lv_scr_act());
		lv_obj_align(labelClock, LV_ALIGN_OUT_RIGHT_TOP, 200 , 10);
		lv_obj_set_style_text_font(labelClock, &dseg15, LV_STATE_DEFAULT);
		lv_obj_set_style_text_color(labelClock, lv_color_white(), LV_STATE_DEFAULT);
		
}

/************************************************************************/
/* TASKS                                                                */
/************************************************************************/

static void task_lcd(void *pvParameters) {
	int px, py;

	//lv_ex_btn_1();
	lv_termostato();

	for (;;)  {
		lv_tick_inc(50);
		lv_task_handler();
		vTaskDelay(50);
	}
}

static void task_rtc(void *pvParameters) {
	/** Configura RTC */
	calendar rtc_initial = {2018, 3, 19, 12, 15, 45 ,1};
	RTC_init(RTC, ID_RTC, rtc_initial, RTC_SR_SEC);
	int32_t delayticks = 0;
	int tick = 1;


	for (;;)  {
		if (xQueueReceive(xQueueClk, &delayticks, (TickType_t) 0)){
			rtc_get_time(RTC, &current_hour,&current_min,&current_sec);
			if (tick == 1){
				printf("%02d:%02d:%02\n", current_hour, current_min, current_sec);
				lv_label_set_text_fmt(labelClock, "%02d:%02d:%02d", current_hour, current_min, current_sec);
				tick = 0;
				} else {
				printf("%02d:%02d:%02\n", current_hour, current_min, current_sec);
				lv_label_set_text_fmt(labelClock, "%02d:%02d:%02d", current_hour, current_min, current_sec);
				tick = 1;
			}
		}
	}
}

/************************************************************************/
/* configs                                                              */
/************************************************************************/

void RTC_init(Rtc *rtc, uint32_t id_rtc, calendar t, uint32_t irq_type) {
	/* Configura o PMC */
	pmc_enable_periph_clk(ID_RTC);

	/* Default RTC configuration, 24-hour mode */
	rtc_set_hour_mode(rtc, 0);

	/* Configura data e hora manualmente */
	rtc_set_date(rtc, t.year, t.month, t.day, t.week);
	rtc_set_time(rtc, t.hour, t.minute, t.second);

	/* Configure RTC interrupts */
	NVIC_DisableIRQ(id_rtc);
	NVIC_ClearPendingIRQ(id_rtc);
	NVIC_SetPriority(id_rtc, 4);
	NVIC_EnableIRQ(id_rtc);

	/* Ativa interrupcao via alarme */
	rtc_enable_interrupt(rtc,  irq_type);
}

static void configure_lcd(void) {
	/**LCD pin configure on SPI*/
	pio_configure_pin(LCD_SPI_MISO_PIO, LCD_SPI_MISO_FLAGS);  //
	pio_configure_pin(LCD_SPI_MOSI_PIO, LCD_SPI_MOSI_FLAGS);
	pio_configure_pin(LCD_SPI_SPCK_PIO, LCD_SPI_SPCK_FLAGS);
	pio_configure_pin(LCD_SPI_NPCS_PIO, LCD_SPI_NPCS_FLAGS);
	pio_configure_pin(LCD_SPI_RESET_PIO, LCD_SPI_RESET_FLAGS);
	pio_configure_pin(LCD_SPI_CDS_PIO, LCD_SPI_CDS_FLAGS);
	
	ili9341_init();
	ili9341_backlight_on();
}

static void configure_console(void) {
	const usart_serial_options_t uart_serial_options = {
		.baudrate = USART_SERIAL_EXAMPLE_BAUDRATE,
		.charlength = USART_SERIAL_CHAR_LENGTH,
		.paritytype = USART_SERIAL_PARITY,
		.stopbits = USART_SERIAL_STOP_BIT,
	};

	/* Configure console UART. */
	stdio_serial_init(CONSOLE_UART, &uart_serial_options);

	/* Specify that stdout should not be buffered. */
	setbuf(stdout, NULL);
}

/************************************************************************/
/* port lvgl                                                            */
/************************************************************************/

void my_flush_cb(lv_disp_drv_t * disp_drv, const lv_area_t * area, lv_color_t * color_p) {
	ili9341_set_top_left_limit(area->x1, area->y1);   ili9341_set_bottom_right_limit(area->x2, area->y2);
	ili9341_copy_pixels_to_screen(color_p,  (area->x2 + 1 - area->x1) * (area->y2 + 1 - area->y1));
	
	/* IMPORTANT!!!
	* Inform the graphics library that you are ready with the flushing*/
	lv_disp_flush_ready(disp_drv);
}

void my_input_read(lv_indev_drv_t * drv, lv_indev_data_t*data) {
	int px, py, pressed;
	
	if (readPoint(&px, &py))
		data->state = LV_INDEV_STATE_PRESSED;
	else
		data->state = LV_INDEV_STATE_RELEASED; 
	
	data->point.x = px;
	data->point.y = py;
}

void configure_lvgl(void) {
	lv_init();
	lv_disp_draw_buf_init(&disp_buf, buf_1, NULL, LV_HOR_RES_MAX * LV_VER_RES_MAX);
	
	lv_disp_drv_init(&disp_drv);            /*Basic initialization*/
	disp_drv.draw_buf = &disp_buf;          /*Set an initialized buffer*/
	disp_drv.flush_cb = my_flush_cb;        /*Set a flush callback to draw to the display*/
	disp_drv.hor_res = LV_HOR_RES_MAX;      /*Set the horizontal resolution in pixels*/
	disp_drv.ver_res = LV_VER_RES_MAX;      /*Set the vertical resolution in pixels*/

	lv_disp_t * disp;
	disp = lv_disp_drv_register(&disp_drv); /*Register the driver and save the created display objects*/
	
	/* Init input on LVGL */
	lv_indev_drv_init(&indev_drv);
	indev_drv.type = LV_INDEV_TYPE_POINTER;
	indev_drv.read_cb = my_input_read;
	lv_indev_t * my_indev = lv_indev_drv_register(&indev_drv);
}

/************************************************************************/
/* main                                                                 */
/************************************************************************/
int main(void) {
	/* board and sys init */
	board_init();
	sysclk_init();
	configure_console();

	/* LCd, touch and lvgl init*/
	configure_lcd();
	configure_touch();
	configure_lvgl();
	
	xQueueClk = xQueueCreate(32, sizeof(uint32_t));

	/* Create task to control oled */
	if (xTaskCreate(task_lcd, "LCD", TASK_LCD_STACK_SIZE, NULL, TASK_LCD_STACK_PRIORITY, NULL) != pdPASS) {
		printf("Failed to create lcd task\r\n");
	}
	
	if (xTaskCreate(task_rtc, "RTC", TASK_RTC_STACK_SIZE, NULL, TASK_RTC_STACK_PRIORITY, NULL) != pdPASS) {
		printf("Failed to create rtc task\r\n");
	}
	
	/* Start the scheduler. */
	vTaskStartScheduler();

	while(1){ }
}