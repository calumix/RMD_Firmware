/*
 * channel.c
 *
 *  Created on: Mar. 31, 2026
 *      Author: slocke
 *
 *  This has all the required functions to control the state of a channel
 *
 */
#include "peripherals.h"
#include "FreeRTOS.h"
#include "channel.h"
#include "env.h"
#include "pin_mux.h"
#include "fsl_gpio.h"
#include "mcp3551.h"
#include <math.h>

#define IO_EXP_LED_Gn		(1<<0)
#define IO_EXP_LED_Rn		(1<<1)
#define IO_EXP_RESETn		(1<<2)
#define IO_EXP_SETn			(1<<3)
#define IO_EXP_POLARITY		(1<<4)
#define IO_EXP_TIA_GAIN_1	(1<<5)
#define IO_EXP_TIA_GAIN_0	(1<<6)

struct range_map ranges[NUM_RANGES]={
		{CH_RANGE_1UA,1,1000000.0},
		{CH_RANGE_10UA,10,100000.0},
		{CH_RANGE_100UA,100,10000.0},
		{CH_RANGE_1MA,1000,1000.0},
};

static const uint8_t dig_pot_addr[NUM_CHANNELS] = {0x18, 0x4c, 0x1a, 0x4e};
/* The two drop-in IO expander variants differ only in their fixed address bits:
 * TCA9554 answers at 0x20 + A2A1A0, TCA9554A at 0x38 + A2A1A0. Boards from
 * different assemblers have carried either, so ChannelInit probes both. */
static const uint8_t io_exp_addr_9554a[NUM_CHANNELS] = {0x38,0x39, 0x3a, 0x3b};
static const uint8_t io_exp_addr_9554[NUM_CHANNELS] = {0x20,0x21, 0x22, 0x23};
static struct mcp3551 spi_dev;
static struct adc meas_adc;
static struct adc rtd_adc;

static struct channel channels[NUM_CHANNELS];

#define R_TOP			270.0
#define AV				101.0		//nominal instr amp gain
#define R_AB			10000.0
#define RDAC_COUNTS		64
#define VOLT_SET_REF	1.65

uint16_t ChannelRangeNumToUa(enum channel_range r){
	int i;
	for(i=0;i<NUM_RANGES;i++){
		if(ranges[i].range == r)
			return ranges[i].range_ua;
	}
	return ranges[NUM_RANGES-1].range_ua;
}

enum channel_range ChannelRangeUaToNum(uint16_t ua){
	int i;
	//assume largest range to start.
	uint16_t selected_index=NUM_RANGES-1;
	//loop from second largest (3) down to lowest (1).
	for(i=NUM_RANGES-1;i>0;i--){
		if(ranges[i-1].range_ua < ua){
			return ranges[selected_index].range;	//use currently selected index
		}
		else selected_index = i-1;
	}
	return ranges[selected_index].range;
}

float ChannelRangeNumToScale(enum channel_range r){
	int i;
	for(i=0;i<NUM_RANGES;i++){
		if(ranges[i].range == r)
			return ranges[i].scale;
	}
	return ranges[NUM_RANGES-1].scale;
}

static void _setvoltage(struct channel * ch, float voltage){
	double calc;
	uint8_t rdac;
	bool negative;

	/* voltage is +/-, and is set from ~0 to ~20mV. Actual maximum varies
	 * because of tolerance of pot
	 * Voltage is calculated as:
	 * Vset = (+/-1.65 * Rwb/(750k + Rab*(1+tol)))/Av
	 * where Rab is 10k, Rwb is (Rab*rdac/64)
	 *
	 * Vset = (+/-1.65 * (rdac/64) * (1/((750k/Rab) + (1+tol))))/Av
	 * So:
	 * D = (Vset*Av)*64 * (750k/Rab + (1+tol))/(+/- 1.65)
	 */
	calc = voltage * AV * RDAC_COUNTS;
	calc *= R_TOP/R_AB + (1+ch->pot.tolerance);
	calc /= VOLT_SET_REF;

	negative = calc < 0;
	if(negative) calc *= -1;
	/* The sign lives on the expander. A channel with no expander still gets its
	 * pot written below (so the bias magnitude is a known 0 at init), but must
	 * not be addressed at io_exp.address 0: that is the I2C general-call address. */
	if(ch->io_exp_ok){
		if(negative) tca9554_set(&ch->io_exp,IO_EXP_POLARITY);
		else tca9554_clear(&ch->io_exp,IO_EXP_POLARITY);
	}
	rdac = calc + 0.5;
	if(rdac >= RDAC_COUNTS) rdac = RDAC_COUNTS-1;
	ad5258_write_rdac(&ch->pot,rdac);
}

/* Read the bias back: sign from the expander's POLARITY pin, magnitude from the
 * pot. Returns 0 (and leaves *voltage alone) when the expander did not answer,
 * because then the sign is simply unknown -- decoding it from whatever was on
 * the stack is what made an unreachable expander look like a working channel. */
static int _getvoltage(struct channel * ch, float * voltage){
	uint8_t io;
	uint8_t rdac;
	double calc;

	if(!tca9554_read(&ch->io_exp,&io)) return 0;
	calc = VOLT_SET_REF;
	if(io & IO_EXP_POLARITY){
		calc *= -1;
	}

	/* Same rule for the pot: a magnitude we never read is not a magnitude. */
	if(!ad5258_read_rdac(&ch->pot,&rdac)) return 0;
	calc *= rdac;
	calc /= RDAC_COUNTS;
	calc /= R_TOP/R_AB + (1+ch->pot.tolerance);
	*voltage = calc/AV;
	return 1;
}

/* How many times an expander transfer that MUST land is attempted before the
 * channel is given up on. One I2C glitch must not retire a healthy channel, but
 * three in a row means the expander has gone away. The attempts are one tick
 * apart: configTICK_RATE_HZ is 100, so pdMS_TO_TICKS() of anything under 10 ms
 * is ZERO and vTaskDelay(0) only yields -- DIV_ROUND_UP gives the 1-tick floor. */
#define IO_EXP_TRIES		3
#define IO_EXP_RETRY_DELAY	DIV_ROUND_UP(2,portTICK_PERIOD_MS)

/* Retire a channel whose expander has stopped answering: never un-isolated
 * again, CH ack reports range 0, IOEXP reports NONE. Loud, never plausible. */
static void _retire(struct channel * ch){
	ch->io_exp_ok = 0;
	ch->isolate = 1;
}

/* Pulse the latching relay's SET or RESET coil for 20 ms. Both edges of the
 * pulse must land: the coil is pulse-rated, so a line driven low must come back
 * high, and a pulse that never started must not be recorded as a relay that
 * moved. Each edge is retried; if the expander still does not answer, the
 * channel is retired rather than reported healthy with the coil possibly held
 * energised. */
static void _relay(struct channel * ch, bool isolate){
	uint8_t bit;
	int i;
	bit = IO_EXP_SETn;
	ch->isolate = 0;
	if(isolate){
		bit = IO_EXP_RESETn;
		ch->isolate = 1;
	}

	for(i=0;i<IO_EXP_TRIES;i++){
		if(tca9554_clear(&ch->io_exp,bit)) break;
		vTaskDelay(IO_EXP_RETRY_DELAY);
	}
	if(i == IO_EXP_TRIES){
		_retire(ch);	//coil never driven: nothing to release
		return;
	}
	vTaskDelay(DIV_ROUND_UP(20,portTICK_PERIOD_MS));
	for(i=0;i<IO_EXP_TRIES;i++){
		if(tca9554_set(&ch->io_exp,bit)) return;
		vTaskDelay(IO_EXP_RETRY_DELAY);
	}
	_retire(ch);
}

/* Does an expander answer at this address? Tries a few times, one tick apart,
 * because a single arbitration or start/stop glitch on the one probe transfer
 * must not mark a fitted part as missing until the next power cycle. On a board
 * fitted with the OTHER variant every attempt at this address NAKs, which costs
 * ~30 ms per channel (~120 ms of boot) -- the relay pulses already cost 80 ms. */
static int _probe_io_exp(struct tca9554 * io_exp, uint8_t address){
	int i;
	io_exp->address = address;
	for(i=0;i<IO_EXP_TRIES;i++){
		if(tca9554_probe(io_exp)) return 1;
		vTaskDelay(IO_EXP_RETRY_DELAY);
	}
	return 0;
}

static void _setrange(struct channel * ch, enum channel_range range){
	switch(range){
		case CH_RANGE_1MA:
			tca9554_clear(&ch->io_exp, IO_EXP_TIA_GAIN_1 | IO_EXP_TIA_GAIN_0);
			break;
		case CH_RANGE_100UA:
			tca9554_clear(&ch->io_exp, IO_EXP_TIA_GAIN_1);
			tca9554_set(&ch->io_exp, IO_EXP_TIA_GAIN_0);
			break;
		case CH_RANGE_10UA:
			tca9554_set(&ch->io_exp, IO_EXP_TIA_GAIN_1);
			tca9554_clear(&ch->io_exp, IO_EXP_TIA_GAIN_0);
			break;
		case CH_RANGE_1UA:
			tca9554_set(&ch->io_exp, IO_EXP_TIA_GAIN_1 | IO_EXP_TIA_GAIN_0);
			break;
	}
}

/* Read the TIA gain pins back off the expander and decode the range. Returns 0
 * (and leaves *range alone) when the expander did not answer: the current's
 * scale is then unknown, and the caller must not invent one. */
static int _getrange(struct channel * ch, enum channel_range * range){
	uint8_t io;
	if(!tca9554_read(&ch->io_exp,&io)) return 0;

	if((io & IO_EXP_TIA_GAIN_0) && (io & IO_EXP_TIA_GAIN_1))
		*range = CH_RANGE_1UA;
	else if((io & IO_EXP_TIA_GAIN_1))
		*range = CH_RANGE_10UA;
	else if((io & IO_EXP_TIA_GAIN_0))
		*range = CH_RANGE_100UA;
	else
		*range = CH_RANGE_1MA;

	return 1;
}

static int32_t _adc_read_chan(struct adc * a, uint8_t grp, uint8_t chan){
	int32_t val;
	xSemaphoreTake(a->lock,portMAX_DELAY);
	//select the correct group
	GPIO_PinWrite(GPIO,a->group_port,a->group_pin,grp);
	//select the correct channel
	switch(chan){
		case 0:
			GPIO_PinWrite(GPIO,a->ch_sel0_port,a->ch_sel0_pin,0);
			GPIO_PinWrite(GPIO,a->ch_sel1_port,a->ch_sel1_pin,0);
			break;
		case 1:
			GPIO_PinWrite(GPIO,a->ch_sel0_port,a->ch_sel0_pin,1);
			GPIO_PinWrite(GPIO,a->ch_sel1_port,a->ch_sel1_pin,0);
			break;
		case 2:
			GPIO_PinWrite(GPIO,a->ch_sel0_port,a->ch_sel0_pin,0);
			GPIO_PinWrite(GPIO,a->ch_sel1_port,a->ch_sel1_pin,1);
			break;
		default:
			GPIO_PinWrite(GPIO,a->ch_sel0_port,a->ch_sel0_pin,1);
			GPIO_PinWrite(GPIO,a->ch_sel1_port,a->ch_sel1_pin,1);
			break;
	}
	//give a short stabilization time
	vTaskDelay(1);

	mcp3551_start(a->dev,a->cs_port,a->cs_pin);
	/* We know it takes at least 73 ms for the conversion, so delay at least that long */
	vTaskDelay(pdMS_TO_TICKS(75));
	mcp3551_read(a->dev,a->cs_port,a->cs_pin,&val);
	xSemaphoreGive(a->lock);

	return val;

}


float ChannelReadVoltage(uint8_t channel_num){
	float f;

	if(channels[channel_num].isolate){
		return 0;
	}

	f = _adc_read_chan(&meas_adc,MEAS_ADC_VOLT_GROUP,channel_num);

	//convert value
	f *= 3.3/2097151.0/AV;

	return f;
}

float ChannelReadCurrent(uint8_t channel_num){
	float f;

	if(channels[channel_num].isolate){
		return 0;
	}

	enum channel_range range;

	f = _adc_read_chan(&meas_adc,MEAS_ADC_CURR_GROUP,channel_num);

	//convert value
	f *= -3.3/2097151.0;	//Due to TIA inversion, current is always negative of what is real
	if(!_getrange(&channels[channel_num],&range)){
		/* The expander that selects the TIA gain did not answer, so this ADC count
		 * has no known scale. NaN, never a plausible number: a wrong-by-10x current
		 * streamed as data is exactly the failure this guards against. */
		return NAN;
	}
	f /= ChannelRangeNumToScale(range);

	return f;
}

float ChannelReadResistance(uint8_t channel_num, float * voltage, float * current){
	float v,c;
	if(channels[channel_num].isolate){
		return NAN;
	}

	v = ChannelReadVoltage(channel_num);
	c = ChannelReadCurrent(channel_num);
	if(voltage != NULL) *voltage = v;
	if(current != NULL) *current = c;

	return v/c;
}

float ChannelReadRTD(uint8_t channel_num){
	float v_rtd;
	float v_ref;
	v_rtd = _adc_read_chan(&rtd_adc,RTD_ADC_RTD_GROUP,channel_num);
	v_ref = _adc_read_chan(&rtd_adc,RTD_ADC_REF_GROUP,channel_num);

	return ((v_rtd * 3000.0)/(v_ref * 100.0 * (1+100/3.4))-1)/0.00385;
}


int ChannelGetState(uint8_t channel_num, struct channel_state *state){
	struct channel * ch = &channels[channel_num];
	if(state != NULL){
		state->io_exp_ok = ch->io_exp_ok
				&& _getrange(ch,&state->range)
				&& _getvoltage(ch,&state->voltage);
		if(!state->io_exp_ok){
			/* Placeholders only; the CH ack reports range 0 for this case. */
			state->range = CH_RANGE_1MA;
			state->voltage = 0.0f;
		}
	}
	return ch->isolate;
}

void ChannelSetState(uint8_t channel_num, const struct channel_state *state,bool isolate){
	struct channel * ch = &channels[channel_num];
	if(!ch->io_exp_ok){
		/* No expander answered at init: the relay, the polarity and the TIA gain
		 * all hang off it, so nothing requested here can actually be set. Refuse
		 * to un-isolate, so MeasTask never streams a number whose sign and scale
		 * were never applied. The CH ack shows "off" and range 0. */
		ch->isolate = 1;
		return;
	}
	_setvoltage(ch,state->voltage);
	_setrange(ch,state->range);
	if(isolate != ch->isolate)
		_relay(ch,isolate);
}

void ChannelAutoRange(uint8_t channel_num, float max_voltage, float max_resistance){
	struct channel_state s;

	s.voltage = max_voltage;
	s.range = ChannelRangeUaToNum(max_voltage * 1000000.0 * 1.05 / max_resistance);
	ChannelSetState(channel_num,&s,0);

}

void ChannelInit(void){
	int i;
	int32_t val;

	/* The lock every expander output change takes, before the first change. */
	tca9554_init();

	/* We share the SPI device between both the measurement and RTD adc, so only one init */
	mcp3551_init(&spi_dev);

	meas_adc.dev = &spi_dev;
	meas_adc.cs_port = BOARD_INITPINS_MEASURE_CSn_PORT;
	meas_adc.cs_pin = BOARD_INITPINS_MEASURE_CSn_PIN;
	meas_adc.ch_sel0_port = BOARD_INITPINS_MEASURE_CH_SEL0_PORT;
	meas_adc.ch_sel0_pin =  BOARD_INITPINS_MEASURE_CH_SEL0_PIN;
	meas_adc.ch_sel1_port = BOARD_INITPINS_MEASURE_CH_SEL1_PORT;
	meas_adc.ch_sel1_pin =  BOARD_INITPINS_MEASURE_CH_SEL1_PIN;
	meas_adc.group_port = BOARD_INITPINS_VOLT_CURRn_PORT;
	meas_adc.group_pin = BOARD_INITPINS_VOLT_CURRn_PIN;
	meas_adc.lock = xSemaphoreCreateMutex();

	rtd_adc.dev = &spi_dev;
	rtd_adc.cs_port = BOARD_INITPINS_RTD_CSn_PORT;
	rtd_adc.cs_pin = BOARD_INITPINS_RTD_CSn_PIN;
	rtd_adc.ch_sel0_port = BOARD_INITPINS_RTD_CH_SEL0_PORT;
	rtd_adc.ch_sel0_pin =  BOARD_INITPINS_RTD_CH_SEL0_PIN;
	rtd_adc.ch_sel1_port = BOARD_INITPINS_RTD_CH_SEL1_PORT;
	rtd_adc.ch_sel1_pin =  BOARD_INITPINS_RTD_CH_SEL1_PIN;
	rtd_adc.group_port = BOARD_INITPINS_RTD_REFn_PORT;
	rtd_adc.group_pin = BOARD_INITPINS_RTD_REFn_PIN;
	rtd_adc.lock = xSemaphoreCreateMutex();

	//perform one read to get the ADC into the sleep state.
	mcp3551_read(&spi_dev,meas_adc.cs_port,meas_adc.cs_pin,&val);
	mcp3551_read(&spi_dev,rtd_adc.cs_port,rtd_adc.cs_pin,&val);

	for(i=0;i<NUM_CHANNELS;i++){
		channels[i].io_exp.i2c_dev = &FLEXCOMM_I2C_rtosHandle;
		channels[i].pot.address = dig_pot_addr[i];
		channels[i].pot.i2c_dev =  &FLEXCOMM_I2C_rtosHandle;
		/* Isolated until the relay below is actually driven. A channel with no
		 * expander never gets there and must not start life streaming. */
		channels[i].isolate = 1;

		/* Auto-detect the expander variant: TCA9554 at 0x20+n, else TCA9554A at
		 * 0x38+n. A channel whose expander answers at neither is marked
		 * io_exp_ok = 0 and stays isolated for good (ChannelSetState refuses to
		 * un-isolate it); $POEI,IOEXP reports what was found. */
		channels[i].io_exp_ok = _probe_io_exp(&channels[i].io_exp, io_exp_addr_9554[i])
				|| _probe_io_exp(&channels[i].io_exp, io_exp_addr_9554a[i]);
		if(!channels[i].io_exp_ok){
			channels[i].io_exp.address = 0;
		}

		if(channels[i].io_exp_ok){
			tca9554_write(&channels[i].io_exp,IO_EXP_LED_Gn | IO_EXP_LED_Rn | IO_EXP_RESETn | IO_EXP_SETn);
			tca9554_direction(&channels[i].io_exp,0x80);
			_relay(&channels[i],1);
		}

		ad5258_load_tolerance(&channels[i].pot);
		ad5258_set_wp(&channels[i].pot, 0);

		_setvoltage(&channels[i],0.0);

		if(channels[i].io_exp_ok){
			tca9554_clear(&channels[i].io_exp, IO_EXP_LED_Gn);	//green LED on: this channel is alive
		}
	}

}

uint8_t ChannelIoExpAddress(uint8_t channel_num){
	if(channel_num >= NUM_CHANNELS) return 0;
	return channels[channel_num].io_exp_ok ? channels[channel_num].io_exp.address : 0;
}

/*
 * The LEDs. Each channel has a green and a red one on two pins of its IO
 * expander, the chip whose other pins drive that channel's relay, bias polarity
 * and range. Both are lit by driving the pin LOW. Every change here is one
 * read-modify-write under the expander lock, so it can never put back another
 * pin of the chip (tca9554.c).
 */

/* Is any channel on, and so being measured? */
bool ChannelAnyOn(void){
	int i;
	for(i=0;i<NUM_CHANNELS;i++){
		if(!channels[i].isolate) return true;
	}
	return false;
}

/* Light or darken one channel's two LEDs. Returns 0 when the channel has no
 * expander, or its expander did not answer. */
int ChannelSetLeds(uint8_t channel_num, bool green, bool red){
	struct channel * ch;
	uint8_t high = 0;
	uint8_t low = 0;

	if(channel_num >= NUM_CHANNELS) return 0;
	ch = &channels[channel_num];
	if(!ch->io_exp_ok) return 0;

	if(green) low |= IO_EXP_LED_Gn; else high |= IO_EXP_LED_Gn;
	if(red) low |= IO_EXP_LED_Rn; else high |= IO_EXP_LED_Rn;
	return tca9554_update(&ch->io_exp,high,low);
}

/* The resting state ChannelInit leaves: green lit on every channel that has an
 * expander ("this channel is alive"), red dark. */
void ChannelLedsRest(void){
	int i;
	for(i=0;i<NUM_CHANNELS;i++){
		ChannelSetLeds(i,true,false);
	}
}


