// HRTF Spatialized Audio
//
// See README.md for more information
//
// Written by Ryan Huffman <ryanhuffman@gmail.com>


#include "SDL2/include/SDL.h"
#include <stdio.h>
#include <stdbool.h>
#include <math.h>

#include "kiss_fft.h"

#include "hrtf.h"

const char HRTF_FILE_FORMAT_MIT[] = "mit/elev%d/H%de%03da.wav";
const char AUDIO_FILE[] = "./beep.wav";
const char BEE_FILE[] = "./fail-buzzer-01.wav";
const char StarWar_FILE[] = "./StarWars3.wav";
const char Train_FILE[] = "./train-whistle-01.wav";

const float FPS = 60.0f;
const float FRAME_TIME = 16.6666667f;   // 1000 / FPS

const int NUM_SAMPLES_PER_FILL = 512;
const int SAMPLE_SIZE = sizeof(float);
const int FFT_POINTS = 512;             // NUM_SAMPLES_PER_FILL

const int SAMPLE_RATE = 44100;

// Configs for forward and inverse FFT
kiss_fft_cfg cfg_forward;
kiss_fft_cfg cfg_inverse;


// Number of samples stored in the audio file
int total_samples = 0;

// starting and ending azimuths
int start = 10, finish = 100;
int userC;
int jumpC;
// FFT storage for convolution with HRTFs during playback
kiss_fft_cpx* audio_kiss_buf;    // Audio data, time domain
kiss_fft_cpx* audio_kiss_freq;   // Audio data, stores a single sample, freq domain
kiss_fft_cpx* audio_kiss_freq_l; // Audio sample multiplied by HRTF, left ear
kiss_fft_cpx* audio_kiss_freq_r; // Audio sample multiplied by HRTF, right ear
kiss_fft_cpx* audio_kiss_time_l; // Final, convolved audio sample, left ear
kiss_fft_cpx* audio_kiss_time_r; // Final, convolved audio sample, right ear


// HRTF data for each point on the horizontal plane (0 ... 180)
const int AZIMUTH_CNT = 37;
const int AZIMUTH_INCREMENT_DEGREES = 5;
hrtf_data hrtfs[37];                        // AZIMUTH CNT


// buf_len should be the number of data point in the stereo `buf`
// Each sample should have 2 data points; 1 for each ear
void init_hrtf_data(hrtf_data* data, float* buf, int buf_len, int azimuth, int elevation) {
    // Initialize properties
    data->azimuth = azimuth;
    data->elevation = elevation;

    // Not really necessary to hold on to the HRIR data
    data->hrir_l = malloc(sizeof(kiss_fft_cpx) * NUM_SAMPLES_PER_FILL);
    data->hrir_r = malloc(sizeof(kiss_fft_cpx) * NUM_SAMPLES_PER_FILL);

    data->hrtf_l = malloc(sizeof(kiss_fft_cpx) * NUM_SAMPLES_PER_FILL);
    data->hrtf_r = malloc(sizeof(kiss_fft_cpx) * NUM_SAMPLES_PER_FILL);

    for (int i = 0; i < NUM_SAMPLES_PER_FILL; i++) {
        if (i < buf_len / 2) {
            data->hrir_l[i].r = buf[i * 2];
            data->hrir_r[i].r = buf[(i * 2) + 1];
        } else {
            data->hrir_l[i].r = 0;
            data->hrir_r[i].r = 0;
        }
        data->hrir_l[i].i = 0;
        data->hrir_r[i].i = 0;
    }

    kiss_fft(cfg_forward, data->hrir_l, data->hrtf_l);
    kiss_fft(cfg_forward, data->hrir_r, data->hrtf_r);
}

void free_hrtf_data(hrtf_data* data) {
    free(data->hrir_l);
    free(data->hrir_r);
    free(data->hrtf_l);
    free(data->hrtf_r);
}

// udata: user data
// stream: stream to copy into
// len: number of bytes to copy into stream
void fill_audio(void* udata, Uint8* stream, int len ) {
    
    static int sample = 0;
    static int azimuth = 0;
    if(azimuth < start ) {
        azimuth = start;
    }
    

    bool swap = false;
    static bool reverse = false;

    if (sample >= total_samples) {
        // azimuth += AZIMUTH_INCREMENT_DEGREES;
        if(start != finish) {
        if(reverse) {
            azimuth -= AZIMUTH_INCREMENT_DEGREES;
            if(azimuth < start) {
                reverse = false;
                azimuth += AZIMUTH_INCREMENT_DEGREES;
                azimuth += AZIMUTH_INCREMENT_DEGREES;
            }
        } else {
            azimuth += AZIMUTH_INCREMENT_DEGREES;
            if(azimuth > finish) {
                reverse = true;
                azimuth -= AZIMUTH_INCREMENT_DEGREES;
                azimuth -= AZIMUTH_INCREMENT_DEGREES;
            }
        }
        // No reverse for standard path
        if(userC == 1){

            reverse = false;
            azimuth %= 360;
        }

        }
        
        if(jumpC == 1){
            // random geneartor
            int odd = rand() % 100; // genearte anything between 0 and 9
            if(odd > 60){
                if(reverse == false){
                    azimuth += AZIMUTH_INCREMENT_DEGREES;
                }else{
                    azimuth -= AZIMUTH_INCREMENT_DEGREES;
                }
                
            }
            if(odd > 85){
                if(reverse == false){
                    azimuth += AZIMUTH_INCREMENT_DEGREES;
                    azimuth += AZIMUTH_INCREMENT_DEGREES;
                }else{
                    azimuth -= AZIMUTH_INCREMENT_DEGREES;
                    azimuth -= AZIMUTH_INCREMENT_DEGREES;
                }
                
            }

        }
        
       
        


        sample = 0;

        printf("Azimuth: %d\n", azimuth);
    }

    int num_samples = len / SAMPLE_SIZE / 2;
    if (total_samples - sample < num_samples) {
        num_samples = total_samples - sample;
    }

    int azimuth_idx = azimuth / AZIMUTH_INCREMENT_DEGREES;
    
    // Because the HRIR recordings are only from 0-180, we swap them when > 180
    if (azimuth > 180) {
        swap = true;
        azimuth_idx = 35 - (azimuth_idx % 37);
    }
    
    hrtf_data* data = &hrtfs[azimuth_idx];

    kiss_fft_cpx* hrtf_l = data->hrtf_l;
    kiss_fft_cpx* hrtf_r = data->hrtf_r;

    // Calculate DFT of sample
    kiss_fft(cfg_forward, audio_kiss_buf + sample, audio_kiss_freq);

    // Apply HRTF
    for (int i = 0; i < num_samples; i++) {
        audio_kiss_freq_l[i].r = (audio_kiss_freq[i].r * hrtf_l->r) - (audio_kiss_freq[i].i * hrtf_l->i);
        audio_kiss_freq_l[i].i = (audio_kiss_freq[i].r * hrtf_l->i) + (audio_kiss_freq[i].i * hrtf_l->r);
    }

    for (int i = 0; i < num_samples; i++) {
        audio_kiss_freq_r[i].r = (audio_kiss_freq[i].r * hrtf_r->r) - (audio_kiss_freq[i].i * hrtf_r->i);
        audio_kiss_freq_r[i].i = (audio_kiss_freq[i].r * hrtf_r->i) + (audio_kiss_freq[i].i * hrtf_r->r);
    }

    // Run reverse FFT to get audio in time domain
    kiss_fft(cfg_inverse, audio_kiss_freq_l, audio_kiss_time_l);
    kiss_fft(cfg_inverse, audio_kiss_freq_r, audio_kiss_time_r);

    // Copy data to stream
    for (int i = 0; i < num_samples; i++) {
        if (swap) {
            ((float*)stream)[i * 2] = audio_kiss_time_r[i].r / FFT_POINTS;
            ((float*)stream)[i * 2 + 1] = audio_kiss_time_l[i].r / FFT_POINTS;
        } else {
            ((float*)stream)[i * 2] = audio_kiss_time_l[i].r / FFT_POINTS;
            ((float*)stream)[i * 2 + 1] = audio_kiss_time_r[i].r / FFT_POINTS;
        }
    }

    sample += num_samples;
}


void print_audio_spec(SDL_AudioSpec* spec) {
    printf("\tFrequency: %u\n", spec->freq);
    const char* sformat;
    switch (spec->format) {
        case AUDIO_S8:
            sformat = S_AUDIO_S8;
            break;
        case AUDIO_U8:
            sformat = S_AUDIO_U8;
            break;

        case AUDIO_S16LSB:
            sformat = S_AUDIO_S16LSB;
            break;
        case AUDIO_S16MSB:
            sformat = S_AUDIO_S16MSB;
            break;

        case AUDIO_U16LSB:
            sformat = S_AUDIO_U16LSB;
            break;
        case AUDIO_U16MSB:
            sformat = S_AUDIO_U16MSB;
            break;

        case AUDIO_S32LSB:
            sformat = S_AUDIO_S32LSB;
            break;
        case AUDIO_S32MSB:
            sformat = S_AUDIO_S32MSB;
            break;

        case AUDIO_F32LSB:
            sformat = S_AUDIO_F32LSB;
            break;
        case AUDIO_F32MSB:
            sformat = S_AUDIO_F32MSB;
            break;

        default:
            sformat = S_AUDIO_UNKNOWN;
            break;
    }
    printf("\tFormat: %s\n", sformat);
    printf("\tChannels: %hhu\n", spec->channels);
    printf("\tSilence: %hhu\n", spec->silence);
    printf("\tSamples: %hu\n", spec->samples);
    printf("\tBuffer Size: %u\n", spec->size);
}

int main(int argc, char* argv[]) {
    int begin, end, sound, choice, jump;
    
    // SDL stuff
    SDL_Init(SDL_INIT_VIDEO | SDL_INIT_AUDIO);
    SDL_Window *window;
    SDL_Surface *windowSurface;

    //SDL_StartTextInput();
    SDL_Surface *image1;
    SDL_Surface *image2;
    SDL_Surface *image3;
    SDL_Surface *image4;
   
    SDL_Surface *currentImage;

    
    window = SDL_CreateWindow("HRTF", SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED, 640, 480,SDL_WINDOW_SHOWN);
    windowSurface = SDL_GetWindowSurface(window);

    image1 = SDL_LoadBMP("test1.bmp");
    image2 = SDL_LoadBMP("test2.bmp");
    image3 = SDL_LoadBMP("choosePath.bmp");

    currentImage = image1;

    bool isRunning = true;
    SDL_Event ev;

    while(isRunning){
        while (SDL_PollEvent(&ev) !=0)
        {
            if(ev.type == SDL_QUIT){
                isRunning = false;
            }

            else if(ev.type == SDL_MOUSEBUTTONUP){
                if(ev.button.clicks == 1){
                    choice = 1;
                }
                if(ev.button.clicks > 1){
                    currentImage = image2;
                    choice = 2;
                }
            }
            else if(ev.type == SDL_MOUSEBUTTONUP){
                if(ev.button.button == SDL_BUTTON_LEFT){
                    currentImage = image1;
                }
                if(ev.button.clicks == SDL_BUTTON_RIGHT){
                    currentImage = image3;
                    
                }
            }
            
        }
        SDL_BlitSurface(currentImage, NULL, windowSurface, NULL);
        SDL_UpdateWindowSurface(window);
    }
    
    //SDL_StopTextInput();
    SDL_FreeSurface(image1);
    SDL_FreeSurface(image2);
    SDL_DestroyWindow(window);

    
    
    
    // interactive stuff
    //fprintf(stdout, "1 for standard path, 2 for customized path\n ");
    //scanf("%d", &choice);
    if(choice == 2){
        
        fprintf(stdout, "Please enter starting azimuth:\n ");
        scanf("%d", &begin);
        fprintf(stdout, "Please enter ending azimuth:\n ");
        scanf("%d", &end);
        if(begin < 0) {  begin = 0;  }
        if(end > 360) {  end = 360;  }
        start = begin;
        finish = end;
        printf("0 for beep, 1 for StarWar, 2 for train, 3 for bee\n");
        scanf("%d", &sound);
        printf("select 0/1 to enable/disable sound effect\n");
        scanf("%d", &jump);
        jumpC = jump;
    }
    else{
        start = 0;
        finish = 360;
        userC = choice;
    }
   

    SDL_AudioSpec obtained_audio_spec;
    SDL_AudioSpec desired_audio_spec;
    SDL_AudioCVT audio_cvt;
    SDL_AudioCVT hrtf_audio_cvt;

    // Audio output format
    desired_audio_spec.freq = SAMPLE_RATE;
    desired_audio_spec.format = AUDIO_F32;
    desired_audio_spec.channels = 2;
    desired_audio_spec.samples = NUM_SAMPLES_PER_FILL;
    desired_audio_spec.callback = fill_audio;
    desired_audio_spec.userdata = NULL;


    printf("Device count: %d\n", SDL_GetNumAudioDevices(0));

    const char* device_name = SDL_GetAudioDeviceName(0, 0);
    printf("Device name: %s\n", device_name);

    SDL_AudioDeviceID audio_device = SDL_OpenAudioDevice(device_name, 0, &desired_audio_spec, &obtained_audio_spec, 0);

    printf("Desired Audio Spec:\n");
    print_audio_spec(&desired_audio_spec);

    printf("Obtained Audio Spec:\n");
    print_audio_spec(&obtained_audio_spec);

    SDL_AudioSpec* file_audio_spec;
    Uint8* audio_buf;
    Uint32 audio_len;
    Uint8* audio_pos;


    // Open audio file
    file_audio_spec = malloc(sizeof(SDL_AudioSpec));
    if (!file_audio_spec) {
        printf("Failed to allocate audio spec for wav file");
        return 1;
    }

    // specified audio file is used
    if(sound == 0) {
        if (!SDL_LoadWAV(BEE_FILE, file_audio_spec, &audio_buf, &audio_len)) {
            printf("Could not load audio file: %s", BEE_FILE);
            SDL_Quit();
           return 1;
        }
    } else if(sound == 1){
        if (!SDL_LoadWAV(StarWar_FILE, file_audio_spec, &audio_buf, &audio_len)) {
            printf("Could not load audio file: %s", StarWar_FILE);
            SDL_Quit();
           return 1;
        }
    }else if(sound == 2){
        if (!SDL_LoadWAV(Train_FILE, file_audio_spec, &audio_buf, &audio_len)) {
            printf("Could not load audio file: %s", Train_FILE);
            SDL_Quit();
           return 1;
        }
    }
     else {
        if (!SDL_LoadWAV(AUDIO_FILE, file_audio_spec, &audio_buf, &audio_len)) {
            printf("Could not load audio file: %s", AUDIO_FILE);
            SDL_Quit();
           return 1;
        }
    }

    printf("Wav Spec:\n");
    print_audio_spec(file_audio_spec);


    // Use mono, the audio will be stereo when the HRTFs are applied
    SDL_BuildAudioCVT(&audio_cvt,
                      file_audio_spec->format, file_audio_spec->channels, file_audio_spec->freq,
                      obtained_audio_spec.format, 1, obtained_audio_spec.freq);

    printf("About to convert wav\n");
    audio_cvt.buf = malloc(audio_len * audio_cvt.len_mult);
    audio_cvt.len = audio_len;
    memcpy(audio_cvt.buf, audio_buf, audio_len);
    SDL_ConvertAudio(&audio_cvt);
    printf("Converted wav\n");

    free(audio_buf);
    audio_buf = audio_cvt.buf;
    audio_pos = audio_buf;
    audio_len = audio_cvt.len_cvt;

    cfg_forward = kiss_fft_alloc(NUM_SAMPLES_PER_FILL, 0, NULL, NULL);
    cfg_inverse = kiss_fft_alloc(NUM_SAMPLES_PER_FILL, 1, NULL, NULL);

    // This will store the entire audio file
    int num_audio_samples = audio_len / sizeof(float);
    // 0-pad up to the end
    int padded_len = ((num_audio_samples - 1) / NUM_SAMPLES_PER_FILL) * NUM_SAMPLES_PER_FILL;
    audio_kiss_buf = malloc(sizeof(kiss_fft_cpx) * padded_len);
    memset(audio_kiss_buf, 0, padded_len);
    total_samples = padded_len;

    const int FFT_SIZE = sizeof(kiss_fft_cpx) * NUM_SAMPLES_PER_FILL;

    audio_kiss_freq = malloc(FFT_SIZE);
    audio_kiss_freq_l = malloc(FFT_SIZE);
    audio_kiss_freq_r = malloc(FFT_SIZE);
    audio_kiss_time_l = malloc(FFT_SIZE);
    audio_kiss_time_r = malloc(FFT_SIZE);

    memset(audio_kiss_freq, 0, FFT_SIZE);
    memset(audio_kiss_freq_l, 0, FFT_SIZE);
    memset(audio_kiss_freq_r, 0, FFT_SIZE);
    memset(audio_kiss_time_l, 0, FFT_SIZE);
    memset(audio_kiss_time_r, 0, FFT_SIZE);

    for (int i = 0; (i * SAMPLE_SIZE) < audio_len; i++) {
        int idx = i;
        audio_kiss_buf[idx].r = ((float*)audio_buf)[i];
        audio_kiss_buf[idx].i = 0;
    }

    for (int azimuth = 0; azimuth < 37; azimuth++) {
        char filename[100];
        sprintf(filename, HRTF_FILE_FORMAT_MIT, 0, 0, azimuth * 5);
        printf("Loading: %s\n", filename);

        SDL_AudioSpec audiofile_spec;
        Uint8* hrtf_buf;
        Uint32 hrtf_len;

        if (!SDL_LoadWAV(filename, &audiofile_spec, &hrtf_buf, &hrtf_len)) {
            printf("Could not load hrtf file (%s): %s\n", filename, SDL_GetError());
            SDL_Quit();
            return 1;
        }

        SDL_BuildAudioCVT(&hrtf_audio_cvt,
                          audiofile_spec.format, audiofile_spec.channels, audiofile_spec.freq,
                          AUDIO_F32LSB, 2, audiofile_spec.freq);

        hrtf_audio_cvt.buf = malloc(hrtf_len* hrtf_audio_cvt.len_mult);
        hrtf_audio_cvt.len = hrtf_len;
        memcpy(hrtf_audio_cvt.buf, hrtf_buf, hrtf_len);
        SDL_ConvertAudio(&hrtf_audio_cvt);

        init_hrtf_data(&hrtfs[azimuth], (float*)hrtf_audio_cvt.buf,
                       hrtf_len / SAMPLE_SIZE, azimuth * AZIMUTH_INCREMENT_DEGREES, 0);

        free(hrtf_buf);
    }


    SDL_Event event;
    Uint32 time = SDL_GetTicks();
    Uint32 last_frame_time = time;
    bool running = true;

    // Start playing audio
    SDL_PauseAudioDevice(audio_device, 0);

    while (running) {
        Uint32 new_time = SDL_GetTicks();
        time = new_time;

        while (SDL_PollEvent(&event)) {
            if (event.type == SDL_QUIT) {
                running = false;
            }

            if (event.type == SDL_KEYDOWN && event.key.keysym.sym == SDLK_ESCAPE) {
                running = false;
            }

        }

        last_frame_time = time;

        if (time - last_frame_time < FRAME_TIME) {
            Uint32 sleep_time = FRAME_TIME - (time - last_frame_time);
            SDL_Delay(sleep_time);
        }
    }


    // Cleanup
    SDL_DestroyWindow(window);
    SDL_FreeWAV(audio_buf);
    SDL_CloseAudio();

    for (int i = 0; i < AZIMUTH_CNT; i++) {
        free_hrtf_data(&hrtfs[i]);
    }

    SDL_Quit();

    return 0;
}
