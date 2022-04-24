#include <asf.h>

#include "gfx_mono_ug_2832hsweg04.h"
#include "gfx_mono_text.h"
#include "sysfont.h"

/************************************************************************/
/* prototype                                                            */
/************************************************************************/
void io_init(void);
void pisca_led(int n, int t);

/************************************************************************/
/* defines                                                              */
/************************************************************************/

// LED
#define LED1_PIO		 PIOA
#define LED1_PIO_ID		 ID_PIOA
#define LED1_PIO_IDX	 0
#define LED1_PIO_IDX_MASK (1u << LED1_PIO_IDX)

// Botão
#define BUT1_PIO			 PIOD
#define BUT1_PIO_ID		     ID_PIOD
#define BUT1_PIO_IDX		 28
#define BUT1_PIO_IDX_MASK (1u << BUT1_PIO_IDX)

/************************************************************************/
/* variaveis globais                                                    */
/************************************************************************/

int freq = 200;

/* flag */
volatile char but_flag; //

/************************************************************************/
/* handler / callbacks                                                  */
/************************************************************************/

void but_callback(void)
{
	if (pio_get(BUT1_PIO, PIO_INPUT, BUT1_PIO_IDX_MASK)) {
		// PINO == 1 --> Borda de subida
		but_flag = 1;
		} else {
		// PINO == 0 --> Borda de descida
		
	}
}

/************************************************************************/
/* funções                                                              */
/************************************************************************/

// pisca led N vez no periodo T
void pisca_led(int n, int t){
	for (int i=0;i<n;i++){
		pio_clear(LED1_PIO, LED1_PIO_IDX_MASK);
		delay_ms(t);
		pio_set(LED1_PIO, LED1_PIO_IDX_MASK);
		delay_ms(t);
	}
}

void io_init(void){
	// Configura led
	pmc_enable_periph_clk(LED1_PIO_ID);
	pio_configure(LED1_PIO, PIO_OUTPUT_0, LED1_PIO_IDX_MASK, PIO_DEFAULT);
	
	// Inicializa clock do periférico PIO responsavel pelo botao
	pmc_enable_periph_clk(BUT1_PIO_ID);

	// Configura PIO para lidar com o pino do botão como entrada
	// com pull-up
	pio_configure(BUT1_PIO, PIO_INPUT, BUT1_PIO_IDX_MASK, PIO_PULLUP | PIO_DEBOUNCE);
	pio_set_debounce_filter(BUT1_PIO, BUT1_PIO_IDX_MASK, 60);
	
	// Configura interrupção no pino referente ao botao e associa
	// função de callback caso uma interrupção for gerada
	// a função de callback é a: but_callback()
	pio_handler_set(BUT1_PIO,
	BUT1_PIO_ID,
	BUT1_PIO_IDX_MASK,
	PIO_IT_EDGE,
	but_callback);
	
	// Ativa interrupção e limpa primeira IRQ gerada na ativacao
	pio_enable_interrupt(BUT1_PIO, BUT1_PIO_IDX_MASK);
	pio_get_interrupt_status(BUT1_PIO);
	
	// Configura NVIC para receber interrupcoes do PIO do botao
	// com prioridade 4 (quanto mais próximo de 0 maior)
	NVIC_EnableIRQ(BUT1_PIO_ID);
	NVIC_SetPriority(BUT1_PIO_ID, 4); // Prioridade 4
}

/************************************************************************/
/* Main                                                                 */
/************************************************************************/

int main (void)
{
	board_init();
	sysclk_init();
	delay_init();
	
	// Desativa watchdog
	WDT->WDT_MR = WDT_MR_WDDIS;

	// configura botao com interrupcao
	io_init();
	
	char str[10];

  // Init OLED
	gfx_mono_ssd1306_init();
  
  // Escreve na tela um circulo e um texto
	gfx_mono_draw_filled_circle(20, 16, 16, GFX_PIXEL_SET, GFX_WHOLE);
	gfx_mono_draw_string("mundo", 50,16, &sysfont);

  /* Insert application code here, after the board has been initialized. */
	while(1) {
		pisca_led(5, freq);
		if (but_flag == 1){
			delay_ms(300);
			if (but_flag == 1){
				freq += 100;
			}
			else{
				freq -= 100;
			}
			sprintf(str, "%6.0lf", freq);
			gfx_mono_draw_string(str, 0, 0, &sysfont);
			but_flag = 0;
		}
	}
}
