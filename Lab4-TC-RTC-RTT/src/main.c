#include <asf.h>

#include "gfx_mono_ug_2832hsweg04.h"
#include "gfx_mono_text.h"
#include "sysfont.h"

// Configuracoes do OLED
#define LED1_PIO		 PIOA
#define LED1_PIO_ID		 ID_PIOA
#define LED1_PIO_IDX	 0
#define LED1_PIO_IDX_MASK (1u << LED1_PIO_IDX) // esse já está pronto.

#define LED2_PIO		 PIOC
#define LED2_PIO_ID		 ID_PIOC
#define LED2_PIO_IDX	 30
#define LED2_PIO_IDX_MASK (1u << LED2_PIO_IDX)

// Configuracoes do botao1
#define BUT1_PIO			 PIOD
#define BUT1_PIO_ID		     ID_PIOD
#define BUT1_PIO_IDX		 28
#define BUT1_PIO_IDX_MASK (1u << BUT1_PIO_IDX) // esse já está pronto.

/* flag */
volatile char but_flag; 
volatile char flag_tc = 0;

void but_callback(void)
{
	if (pio_get(BUT1_PIO, PIO_INPUT, BUT1_PIO_IDX_MASK)) {
		// PINO == 1 --> Borda de subida
		but_flag = 1;
		} else {
		// PINO == 0 --> Borda de descida
		but_flag = 0;
	}
}

void pin_toggle(Pio *pio, uint32_t mask) {
	if(pio_get_output_data_status(pio, mask))
	pio_clear(pio, mask);
	else
	pio_set(pio,mask);
}

void TC1_Handler(void) {
	/**
	* Devemos indicar ao TC que a interrupção foi satisfeita.
	* Isso é realizado pela leitura do status do periférico
	**/
	volatile uint32_t status = tc_get_status(TC0, 1);

	/** Muda o estado do LED (pisca) **/
	pin_toggle(LED1_PIO, LED1_PIO_IDX_MASK);  
}


void TC_init(Tc * TC, int ID_TC, int TC_CHANNEL, int freq){
	uint32_t ul_div;
	uint32_t ul_tcclks;
	uint32_t ul_sysclk = sysclk_get_cpu_hz();

	uint32_t channel = 1;

	/* Configura o PMC */
	/* O TimerCounter é meio confuso
	o uC possui 3 TCs, cada TC possui 3 canais
	TC0 : ID_TC0, ID_TC1, ID_TC2
	TC1 : ID_TC3, ID_TC4, ID_TC5
	TC2 : ID_TC6, ID_TC7, ID_TC8
	*/
	pmc_enable_periph_clk(ID_TC);

	/** Configura o TC para operar em  freq hz e interrupçcão no RC compare */
	tc_find_mck_divisor(freq, ul_sysclk, &ul_div, &ul_tcclks, ul_sysclk);
	tc_init(TC, TC_CHANNEL, ul_tcclks | TC_CMR_CPCTRG);
	tc_write_rc(TC, TC_CHANNEL, (ul_sysclk / ul_div) / freq);

	/* Configura e ativa interrupçcão no TC canal 0 */
	/* Interrupção no C */
	NVIC_EnableIRQ((IRQn_Type) ID_TC);
	tc_enable_interrupt(TC, TC_CHANNEL, TC_IER_CPCS);
}

static void RTT_init(float freqPrescale, uint32_t IrqNPulses, uint32_t rttIRQSource) {

	uint16_t pllPreScale = (int) (((float) 32768) / freqPrescale);
	
	rtt_sel_source(RTT, false);
	rtt_init(RTT, pllPreScale);
	
	if (rttIRQSource & RTT_MR_ALMIEN) {
		uint32_t ul_previous_time;
		ul_previous_time = rtt_read_timer_value(RTT);
		while (ul_previous_time == rtt_read_timer_value(RTT));
		rtt_write_alarm_time(RTT, IrqNPulses+ul_previous_time);
	}
	
	/* config NVIC */
	NVIC_DisableIRQ(RTT_IRQn);
	NVIC_ClearPendingIRQ(RTT_IRQn);
	NVIC_SetPriority(RTT_IRQn, 4);
	NVIC_EnableIRQ(RTT_IRQn);

	/* Enable RTT interrupt */
	if (rttIRQSource & (RTT_MR_RTTINCIEN | RTT_MR_ALMIEN))
	rtt_enable_interrupt(RTT, rttIRQSource);
	else
	rtt_disable_interrupt(RTT, RTT_MR_RTTINCIEN | RTT_MR_ALMIEN);
	
}

void RTT_Handler(void) {
	uint32_t ul_status;

	/* Get RTT status - ACK */
	ul_status = rtt_get_status(RTT);

	/* IRQ due to Alarm */
	if ((ul_status & RTT_SR_ALMS) == RTT_SR_ALMS) {
		//RTT_init(4, 16,  RTT_MR_RTTINCIEN);
		pin_toggle(LED2_PIO, LED2_PIO_IDX_MASK);
		RTT_init(4, 16, RTT_MR_ALMIEN);
	}
	
	/* IRQ due to Time has changed */
	if ((ul_status & RTT_SR_RTTINC) == RTT_SR_RTTINC) {
		//pin_toggle(LED2_PIO, LED2_PIO_IDX_MASK);    // BLINK Led
	}

}


void init(void){
	
	pmc_enable_periph_clk(LED1_PIO_ID);
	pio_configure(LED1_PIO, PIO_OUTPUT_0, LED1_PIO_IDX_MASK, PIO_DEFAULT);
	
	pmc_enable_periph_clk(LED2_PIO_ID);
	pio_configure(LED2_PIO, PIO_OUTPUT_0, LED2_PIO_IDX_MASK, PIO_DEFAULT);
	
	pmc_enable_periph_clk(BUT1_PIO_ID);
	
	pio_configure(BUT1_PIO, PIO_INPUT, BUT1_PIO_IDX_MASK, PIO_PULLUP | PIO_DEBOUNCE);
	pio_set_debounce_filter(BUT1_PIO, BUT1_PIO_IDX_MASK, 60);
	
	pio_handler_set(BUT1_PIO,
					BUT1_PIO_ID,
					BUT1_PIO_IDX_MASK,
					PIO_IT_EDGE,
					but_callback);
	
	pio_enable_interrupt(BUT1_PIO, BUT1_PIO_IDX_MASK);
	pio_get_interrupt_status(BUT1_PIO);
	
	NVIC_EnableIRQ(BUT1_PIO_ID);
	NVIC_SetPriority(BUT1_PIO_ID, 4); // Prioridade 4

}

int main (void)
{
	board_init();
	sysclk_init();
	delay_init();	
	init();
	
	/** Configura timer TC0, canal 1 */
	TC_init(TC0, ID_TC1, 1, 4);
	tc_start(TC0, 1);  
	
	RTT_init(4, 16, RTT_MR_ALMIEN);

  // Init OLED
	gfx_mono_ssd1306_init();
  
  
	gfx_mono_draw_filled_circle(20, 16, 16, GFX_PIXEL_SET, GFX_WHOLE);
    gfx_mono_draw_string("mundo", 50,16, &sysfont);
  
  

  /* Insert application code here, after the board has been initialized. */
	while(1) {
			// Escreve na tela um circulo e um texto
			
			for(int i=70;i<=120;i+=2){
				gfx_mono_draw_rect(i, 5, 2, 10, GFX_PIXEL_SET);
				delay_ms(10);
				
			}
			
			for(int i=120;i>=70;i-=2){
				gfx_mono_draw_rect(i, 5, 2, 10, GFX_PIXEL_CLR);
				delay_ms(10);
				
			}
			
			if (but_flag){
			}
	}
}
