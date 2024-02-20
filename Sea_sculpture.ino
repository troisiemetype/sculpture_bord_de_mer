#include <MozziGuts.h>
#include <Oscil.h>
#include <ResonantFilter.h>
#include <StateVariable.h>
#include <mozzi_rand.h>

#include <EventDelay.h>

#include <tables/brownnoise8192_int8.h>
#include <tables/pinknoise8192_int8.h>

#include <tables/sin2048_int8.h>


#undef CONTROL_RATE
#define CONTROL_RATE 64

const uint8_t POT_1 = A3;
const uint8_t POT_2 = A2;
const uint8_t POT_3 = A1;
const uint8_t POT_4 = A0;
const uint8_t POT_5 = A10;
const uint8_t POT_6 = A6;
const uint8_t POT_7 = A7;
const uint8_t POT_8 = A8;

int16_t valueA = 0;
int16_t valueB = 0;

uint16_t cutoff1 = 0;
uint16_t res1 = 0;
uint16_t freq1 = 0;
uint16_t amp1 = 0;

uint16_t cutoff2 = 0;
uint16_t res2 = 0;
uint16_t freq2 = 0;
uint16_t amp2 = 0;

// brown noise will be better for waves, pink noise for ambient noise.

Oscil<BROWNNOISE8192_NUM_CELLS, 4096> aNoise(BROWNNOISE8192_DATA);
Oscil<PINKNOISE8192_NUM_CELLS, 4096> bNoise(PINKNOISE8192_DATA);

Oscil<SIN2048_NUM_CELLS, CONTROL_RATE> filter1CutoffMod(SIN2048_DATA);
Oscil<SIN2048_NUM_CELLS, CONTROL_RATE> filter1ResMod(SIN2048_DATA);

Oscil<SIN2048_NUM_CELLS, CONTROL_RATE> filter2CutoffMod(SIN2048_DATA);
Oscil<SIN2048_NUM_CELLS, CONTROL_RATE> filter2ResMod(SIN2048_DATA);

// State variable filter. Potentiometers control the center frequency (i.e. cutoff) and resonance.
StateVariable<LOWPASS> svf1;
StateVariable<LOWPASS> svf2;
uint8_t resonance;

int16_t min = 0;
int16_t max = 0;

EventDelay blinky;

void setup() {
	Serial.begin(115200);

	aNoise.setFreq(1.f);
	bNoise.setFreq(1.f);

	filter1CutoffMod.setFreq(0.083f);
	filter1ResMod.setFreq(0.025f);

	filter2CutoffMod.setFreq(0.083f);
	filter2ResMod.setFreq(0.025f);

	startMozzi(CONTROL_RATE);

	// Shut the two "serial" leds on the pro micro
	PORTB |= (1 << 0);
	PORTD |= (1 << 5);

	blinky.start(500);
}

void updateControl() {

/*
 *	We have one noise, two pots.
 *		One for setting the cutoff frequency.
 *		One for setting the resonnace.
 *	Both controls also have a sine wave that modifies the setting around its midline.
 *	This sine wave has its amplitude and period modified by two other pots.
 *	So that's four pots for one noise channel
 */
	aNoise.setPhase(rand(BROWNNOISE8192_NUM_CELLS));
	bNoise.setPhase(rand(PINKNOISE8192_NUM_CELLS));

//	int16_t cutoff = map(filter1CutoffMod.next(), -128, 127, 60, 180);
	int16_t cutoff = 400;
	// 4096 is too much, it distorts.

	filter1CutoffMod.setFreq((freq1 + 1.f) / 1000.f);
	int16_t amplitude = map(amp1, 0, 1023, 0, 1800);

	cutoff = map(cutoff1, 0, 1023, 50, 4000);

	cutoff += map(filter1CutoffMod.next(), -128, 127, -amplitude, amplitude);

	if(cutoff < 50) cutoff = 50;

	int16_t resonance = 100;
//	resonance = map(filter1ResMod.next(), -128, 127, 80, 180);
	resonance = map(res1, 0, 1023, 10, 185);

	svf1.setResonance(resonance);
	svf1.setCentreFreq(cutoff);


	filter2CutoffMod.setFreq((freq2 + 1.f) / 1000.f);
	amplitude = map(amp2, 0, 1023, 0, 1800);

	cutoff = map(cutoff2, 0, 1023, 50, 4000);

	cutoff += map(filter2CutoffMod.next(), -128, 127, -amplitude, amplitude);

	if(cutoff < 50) cutoff = 50;

//	resonance = map(filter1ResMod.next(), -128, 127, 80, 180);
	resonance = map(res2, 0, 1023, 10, 185);

	svf2.setResonance(resonance);
	svf2.setCentreFreq(cutoff);

/*
	Serial.print(valueB);
	Serial.print('\t');
	Serial.print(cutoff);

	Serial.print('\t');

	Serial.print(valueA);
	Serial.print('\t');
	Serial.println(resonance);
*/
/*
	Serial.print(cutoff);
	Serial.print('\t');
	Serial.print(resonance);
	
	Serial.print('\t');
	
	Serial.print(min);
	Serial.print('\t');
	Serial.println(max);
*/

	if(blinky.ready()){
		blinky.start();
		PORTB ^= (1 << 0);
	}

}

// should return int between -244 & 243. Doesn't with the filter
int updateAudio() {
//	int ret = aNoise.next();
//	int ret = lpf.next(aNoise.next());
//	int ret = lpf.next(bNoise.next());
//	int ret = lpf.next((aNoise.next() + bNoise.next()) / 2);
//	int ret = 0;

	int ret;
	ret = svf1.next(aNoise.next()) >> 2;
	ret += svf2.next(bNoise.next()) >> 2;
/*
	if(abs(ret) > 220){
		PORTB &= ~(1 << 0);			
	} else {
		PORTB |= (1 << 0);
	}
*/
//	ret = aNoise.next() >> 2;
//	ret += bNoise.next() >> 2;
/*
	if(ret < min) min = ret;
	else if(ret > max) max = ret;
*/
//	ret = constrain(ret, -244, 243);

	return ret;
}

void loop() {
	audioHook(); // fills the audio buffer

	cutoff1 = mozziAnalogRead(POT_5);
	res1 = mozziAnalogRead(POT_2);
	freq1 = mozziAnalogRead(POT_3);
	amp1 = mozziAnalogRead(POT_4);

	cutoff2 = mozziAnalogRead(POT_6);
	res2 = mozziAnalogRead(POT_7);
	freq2 = mozziAnalogRead(POT_8);
//	amp2 = mozziAnalogRead(POT_4);
	amp2 = 255;

/*
	Serial.print(valueA);
	Serial.print('\t');
	Serial.println(valueB);
*/
}

