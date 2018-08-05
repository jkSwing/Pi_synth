#include <fluidsynth.h>
#include <stdlib.h>
#include <wiringPi.h>
#include <math.h>

#define abs(x) ((x) < 0 ? -(x) : (x))

struct fx_data_t {
	fluid_synth_t *synth;
	float val;
};
int instrument = 0, octave = 0;
fluid_settings_t* settings;
fluid_synth_t* synth;
fluid_audio_driver_t* adriver = NULL;

void showInst(fluid_sfont_t *sf, int preNum) {
	fluid_preset_t* ps = sf->get_preset(sf, 0, preNum);
	printf("current instrument: No.%d: %s\n", preNum, 
			ps->get_name(ps));
}

int fx_func(void *data, int len, int nin, float **in, int nout, float **out) {
	struct fx_data_t *fx_data = (struct fx_data_t*) data;
	if (fluid_synth_process(fx_data->synth, len, nin, in, nout, out) != 0) {
		return -1;
	}
	for (int i = 0; i < nout; ++i) {
		float *out_i = out[i];
		for (int k = 0; k < len; ++k) {
			int x = out_i[k];
			if (abs(x) < 1e-8) continue;
			int y = x * x / abs(x);
			out_i[k] = x / abs(x) * (1 - exp(y));
		}
	}
	return 0;
}

void octaveControl() {
	if (!digitalRead(25)) {
		++octave;
		if (octave > 1) octave = -1;
		delay(100);
	}
}

int playing[30] = {0};
void noteControl() {
	for (int i = 0; i <= 7; ++i) {
		int note = i + 60 + octave * 12;
		if (!digitalRead(i)) {
			if (!playing[i]) {
				fluid_synth_noteon(synth, 0, note, 127);
				playing[i] = 1;
			}
		}
		else {
			if (playing[i]) {
				fluid_synth_noteoff(synth, 0, note);
				playing[i] = 0;
			}
		}
	}
	for (int i = 21; i <= 24; ++i) {
		int note = i - 13 + 60 + octave * 12;
		if (!digitalRead(i)) {
			if (!playing[i]) {
				fluid_synth_noteon(synth, 0, note, 127);
				playing[i] = 1;
			}
		}
		else {
			if (playing[i]) {
				fluid_synth_noteoff(synth, 0, note);
				playing[i] = 0;
			}
		}
	}
}

int main(int argc, char** argv)
{
	fluid_sfont_t* sfont;
	struct fx_data_t fx_data;
	int sfont_id;
	int ret;

	/* initialize wiringPi and pins */
	wiringPiSetup();
	for (int i = 0; i <= 7; ++i) {
		pinMode(i, INPUT);
		pullUpDnControl(i, PUD_UP);
	}
	for (int i = 21; i <= 25; ++i) {
		pinMode(i, INPUT);
		pullUpDnControl(i, PUD_UP);
	}

	/* Create the settings. */
	settings = new_fluid_settings();
	
	/* Change the settings if necessary*/
	ret = fluid_settings_setstr(settings, "audio.driver", "alsa");
	if (ret == FLUID_FAILED) {
		fprintf(stderr, "cannot change audio driver\n");
		goto cleanUp;
	}
	ret = fluid_settings_setnum(settings, "synth.gain", 1.0);
	if (ret == FLUID_FAILED) {
		fprintf(stderr, "cannot change gain\n");
		goto cleanUp;
	}
	
	/* Create the synthesizer. */
	synth = new_fluid_synth(settings);
	
	fx_data.synth = synth;

	/* Create the audio driver. The synthesizer starts playing as soon
		as the driver is created. */
	adriver = new_fluid_audio_driver(settings, synth);
	// adriver = new_fluid_audio_driver2(settings, fx_func, (void *) &fx_data);
	
	/* Load a SoundFont and reset presets (so that new instruments
	 * get used from the SoundFont) */
	sfont_id = fluid_synth_sfload(synth, "./samples/touhou.sf2", 1);
	if (sfont_id == FLUID_FAILED) {
		fprintf(stderr, "error on open soundfont\n");
		goto cleanUp;
	}
	sfont = fluid_synth_get_sfont_by_id(synth, sfont_id);
	showInst(sfont, instrument);
	
	/* Initialize the random number generator */
	// srand(getpid());
	
	printf("ready\n");

	for (;;) {
		noteControl();
		octaveControl();
		/*
		if (!digitalRead(1)) {
			ret = fluid_synth_program_change(synth, 0, ++instrument);
			if (ret == FLUID_FAILED) {
				fprintf(stderr, "cannot change instrument\n");
				goto cleanUp;
			}
			showInst(sfont, instrument);
			delay(200);
		}
		*/
		delay(100);
	}
	
cleanUp:
	delete_fluid_audio_driver(adriver);
	delete_fluid_synth(synth);
	delete_fluid_settings(settings);
	return 0;
}
