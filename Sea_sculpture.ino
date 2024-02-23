#include <MozziGuts.h>
#include <Oscil.h>
#include <ResonantFilter.h>
#include <StateVariable.h>
#include <mozzi_rand.h>
#include <ADSR.h>

#include <EventDelay.h>

#include <tables/brownnoise8192_int8.h>
#include <tables/pinknoise8192_int8.h>

#include <tables/sin2048_int8.h>


#undef CONTROL_RATE
#define CONTROL_RATE 32

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

uint16_t vol = 0;
uint16_t vol1 = 0;
uint16_t vol2 = 0;

uint16_t cutoff1 = 0;
uint16_t res1 = 0;
uint16_t freq1 = 0;
uint16_t amp1 = 0;

uint16_t cutoff2 = 0;
uint16_t res2 = 0;
uint16_t freq2 = 0;
uint16_t amp2 = 0;

uint16_t intervale = 2000;
uint16_t varIntervale = 1000;
uint16_t varWave1 = 50;
uint16_t varWave2 = 800;

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

ADSR<CONTROL_RATE, AUDIO_RATE> waveIn;
ADSR<CONTROL_RATE, AUDIO_RATE> waveOut;

uint8_t resonance;

int16_t min = 0;
int16_t max = 0;

EventDelay blinky;
EventDelay wave;
EventDelay noiseChange;

//int16_t cutoffA = 800;
int16_t cutoffB = 800;
int16_t cutoffChange = 0;

enum waveState_t{
	IDLE = 0,
	BREAKING_IN,
	SPREADING,
	LEAVING,
} waveState;

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

//	blinky.start(500);

	waveState = IDLE;

	wave.start(250);

	noiseChange.start(3000);
}

void waveHandler(){
	switch(waveState){
		case IDLE:
			if(wave.ready()){
				int16_t var1 = rand(2 * varWave1) - varWave1;
				int16_t var2 = rand(2 * varWave2) - varWave2;
				waveIn.setADLevels(255, 180);
				waveIn.setSustainLevel(180);
				waveIn.setTimes(150 + var1, 150 + var1, 200, 2200 + var2);				
				waveIn.noteOn();
				waveState = BREAKING_IN;
			}
			break;
		case BREAKING_IN:
			if(!waveIn.playing()){
				wave.start(400);
				waveState = SPREADING;
			}
			break;
		case SPREADING:
			if(wave.ready()){
				int16_t var = rand(2 * varWave2) - varWave2;
				waveIn.setADLevels(120, 120);
				waveIn.setSustainLevel(150);
				waveIn.setTimes(1200 + var, 25, 0, 2500 + var);				
				waveIn.noteOn();
				waveState = LEAVING;
			}

			break;
		case LEAVING:
			if(!waveIn.playing()){
				wave.start(intervale + rand(varIntervale) - varIntervale * 2);
				waveState = IDLE;
			}
			break;
		default:

			break;
	}
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

	// We read controls.
	vol = mozziAnalogRead(POT_2);

	cutoff1 = mozziAnalogRead(POT_3);
	res1 = mozziAnalogRead(POT_4);
	intervale = mozziAnalogRead(POT_5);

	cutoff2 = mozziAnalogRead(POT_6);
	res2 = mozziAnalogRead(POT_7);
	freq2 = mozziAnalogRead(POT_8);
//	amp2 = mozziAnalogRead(POT_8);

//	int16_t cutoff = map(filter1CutoffMod.next(), -128, 127, 60, 180);
	int16_t cutoff = 400;
	// 4096 is too much, it distorts.

	vol1 = map(vol, 0, 1023, 0, 512);
	vol2 = map(vol, 0, 1023, 512, 0);

	if(vol1 > 255) vol1 = 255;
	if(vol2 > 255) vol2 = 255;

	intervale = map(intervale, 0, 1023, 2000, 8000);

	filter1CutoffMod.setFreq((freq1 + 1.f) / 1000.f);
	int16_t amplitude = map(amp1, 0, 1023, 0, 1800);

	cutoff = map(cutoff1, 0, 1023, 50, 4000);

//	cutoff += map(filter1CutoffMod.next(), -128, 127, -amplitude, amplitude);
//	if(cutoff < 50) cutoff = 50;

	int16_t resonance = 100;
//	resonance = map(filter1ResMod.next(), -128, 127, 80, 180);
	resonance = map(res1, 0, 1023, 20, 185);

	svf1.setResonance(resonance);
	svf1.setCentreFreq(cutoff);


	filter2CutoffMod.setFreq((freq2 + 1.f) / 1000.f);
//	amplitude = map(amp2, 0, 1023, 0, 1800);
	amplitude = 600;

//	cutoff = map(cutoff2, 0, 1023, 50, 4000);

//	cutoff += map(filter2CutoffMod.next(), -128, 127, -amplitude, amplitude);
//	if(cutoff < 50) cutoff = 50;

//	resonance = map(filter1ResMod.next(), -128, 127, 80, 180);
	resonance = map(res2, 0, 1023, 10, 185);

	cutoffB += cutoffChange;
	if(cutoffB < 50){
		cutoffB = 50;
		cutoffChange = rand(30);
	} else if(cutoffB > 4000){
		cutoffB = 4000;
		cutoffChange = rand(30) * -1;
	}

	svf2.setResonance(resonance);
	svf2.setCentreFreq(cutoffB);

	waveIn.update();

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
//		PORTB ^= (1 << 0);
	}

	waveHandler();

	if(noiseChange.ready()){
		noiseChange.start();
		cutoffChange = rand(30) - 60;
	}

}

// should return int between -244 & 243. Doesn't with the filter
int updateAudio() {
//	int ret = aNoise.next();
//	int ret = lpf.next(aNoise.next());
//	int ret = lpf.next(bNoise.next());
//	int ret = lpf.next((aNoise.next() + bNoise.next()) / 2);
//	int ret = 0;

	int16_t ret = 0;
	ret = svf1.next(aNoise.next()) >> 2;
	ret *= waveIn.next();
	ret >>= 8;
	ret *= vol1;
	ret >>= 8;

	int16_t ret2 = 0;
	ret2 += svf2.next(bNoise.next()) >> 2;
	ret2 *= vol2;
	ret2 >>= 8;

	ret += ret2;
/*
	if(abs(ret) > 200){
		PORTD &= ~(1 << 5);
	} else {
		PORTD |= (1 << 5);
	}
*/
//	ret = aNoise.next() >> 2;
//	ret += bNoise.next() >> 2;
/*
	if(ret < min) min = ret;
	else if(ret > max) max = ret;
*/
	ret = constrain(ret, -244, 243);

	return ret;
}

void loop() {
	audioHook(); // fills the audio buffer

/*
	Serial.print(valueA);
	Serial.print('\t');
	Serial.println(valueB);
*/
}

