//Using SDL and standard IO
#include <SDL.h>
#include <thread>
#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include <string>
#include <sstream>
#include "ThreadPool.h"
#include <iostream>
#include <fstream>

#include "vld.h"

using namespace std;

//Screen dimension constants
const int SCREEN_WIDTH = 800;
const int SCREEN_HEIGHT = 625;

//The window we'll be rendering to
SDL_Window* window = NULL;

//The surface contained by the window
SDL_Surface* screenSurface = NULL;

//SDL Renderer
SDL_Renderer* gRenderer = NULL;
//SDL_Texture to Blit Mandelbrot image
SDL_Texture* texture = NULL;

//Thread poo interface (extern)
ThreadPool poolThread;

//Need Exit?
int quit = 0;
//Catch Events
SDL_Event e;

//Array for put each pixel of set
unsigned char pixelArray[SCREEN_WIDTH*SCREEN_HEIGHT * 4];

//Keep track of the frame count
int frame = 0;
int threadCount = 0;

#pragma region "MandelBrot Variables"
//min real
double MinRe = -2.0;
//max real
double MaxRe = 1.0;
//min imaginary
double MinIm = -1.2;
//max imaginary
double MaxIm = MinIm + (MaxRe - MinRe)*SCREEN_HEIGHT / SCREEN_WIDTH;
//real factor
double Re_factor = (MaxRe - MinRe) / (SCREEN_WIDTH - 1);
//imaginary factor 
double Im_factor = (MaxIm - MinIm) / (SCREEN_HEIGHT - 1);
const unsigned MaxIterations = 128;
#pragma endregion

//Color Palette 512 colours
unsigned char colorPal[MaxIterations * 4];

void draw_madelbrot();

//Task of compute mandelbrot slice to pieces for each thread to work on 
//this is sent to ThreadPool
class SliceComputeTask : Task{

public:
	// is ScreenHeight / NumberOfCores
	double step; 
	//Actual Slice of mandelbrot that the thread will render and calculate
	unsigned index; 
	//Used for wait all threads done
	int stage;

	virtual void *DoWork(){
		stage = Running;

		//Offset of slice
		double y1 = step * index;
		for (unsigned y = y1; y < y1 + step; y++){
			double imA = MaxIm - y * Im_factor;
			for (unsigned x = 0; x < SCREEN_WIDTH; x++){
				double reA = MinRe + x * Re_factor;

				double reB = reA, imB = imA;
				bool isInside = true;
				//offset of pixelBuffer
				const unsigned offset = (SCREEN_WIDTH * 4 * y) + x * 4;
				//offset of Color Palette
				unsigned pOffset = 0;
				unsigned n;

				for (n = 0; n<MaxIterations; n++){
					double reB2 = reB * reB, imB2 = imB * imB;


					if (reB2 + imB2 > 4){
						isInside = false;
						break;
					}
					//outer part of the mandelbrot 
					else{
						//Draw Pixel According with color table
						pixelArray[offset] = colorPal[n];
						pixelArray[offset + 1] = colorPal[n + 1];
						pixelArray[offset + 2] = colorPal[n + 2];
						pixelArray[offset + 3] = SDL_ALPHA_OPAQUE;


					}
					imB = 2 * reB*imB + imA;
					reB = reB2 - imB2 + reA;
				}
				//inside of the mandelbrot
				if (isInside){
					//Draw pixel
					pixelArray[offset] = 0; // blue
					pixelArray[offset + 1] = 0; // green
					pixelArray[offset + 2] = 0; // red
					pixelArray[offset + 3] = SDL_ALPHA_OPAQUE; // alpha

				}
			}
		}
		//Task finished
		stage = Finished;

	};


};

//Pointer to all task
//don't alloc task on each thread
SliceComputeTask *sliceTasks = NULL;




int main(int argc, char* args[]) {

	ifstream inFile;

	inFile.open("threadCount.txt");

	if (!inFile) {
		cerr << "Unable to open file datafile.txt";
		exit(1);   // call system to stop
	}
	int x = 0;
	inFile >> x;
	threadCount = x;

	//Initialize thread Pool

	//initialize Threads
	poolThread.init(threadCount);

	//allocate task's in memory
	sliceTasks = new SliceComputeTask[64];



#pragma region "Init SDL"
	//Initialize SDL
	if (SDL_Init(SDL_INIT_VIDEO) < 0){
		cout << "SDL Could not Initialise" << endl;
	}
	else{
		//Create window
		window = SDL_CreateWindow("Mandelbrot Fractal", 100, 100, SCREEN_WIDTH, SCREEN_HEIGHT, SDL_WINDOW_SHOWN);
		if (window == NULL){
			cout << "Window could not be displayed" << endl;
		}
		else{
			//Get window surface
			screenSurface = SDL_GetWindowSurface(window);
			//initialse renderer
			gRenderer = SDL_CreateRenderer(window, -1, SDL_RENDERER_ACCELERATED);
			if (gRenderer == NULL){
				cout << "Renderer could not be loaded" << endl;
			}
			else{
				//Initialize renderer color
				SDL_SetRenderDrawColor(gRenderer, 0x00, 0x00, 0x00, 0xFF);
			}

			//Create texture for blit Mandelbrot to screen
			SDL_Texture* texture = SDL_CreateTexture
			(
				gRenderer,
				SDL_PIXELFORMAT_ARGB8888,
				SDL_TEXTUREACCESS_STREAMING,
				SCREEN_WIDTH, SCREEN_HEIGHT
			);

#pragma endregion

			//generate color Palette

			for (int k = 0; k < MaxIterations; k++) {

				colorPal[k + 0] = k * 20;
				colorPal[k + 1] = k;
				colorPal[k + 2] = 0;
				colorPal[k + 3] = SDL_ALPHA_OPAQUE;

			}

			while (quit == 0){
				while (SDL_PollEvent(&e) != 0){
					//User requests quit
					if (e.type == SDL_QUIT){
						quit = 1;
					}
				}

				//Draw Mandelbrot
				draw_madelbrot();

				//Update texture
				SDL_UpdateTexture
				(
					texture,
					NULL,
					&pixelArray[0],
					SCREEN_WIDTH * 4
				);
				//Copy to screen
				SDL_RenderCopy(gRenderer, texture, NULL, NULL);
				//Update screen
				SDL_RenderPresent(gRenderer);

				frame++;
			}


		}
	}
	//Destroy window
	SDL_DestroyWindow(window);
	SDL_DestroyTexture(texture);
	poolThread.destroy();
	//Quit SDL subsystems
	SDL_Quit();

	inFile.close();

	return 0;
}



//mandelbrot main function
void draw_madelbrot(){

	//calculate slice size
	double step = SCREEN_HEIGHT / (threadCount * 2);

	//foreach on thread
	for (int i = 0; i < (threadCount * 2); i++){

		//Setup task
		sliceTasks[i].step = step;
		sliceTasks[i].index = i;
		sliceTasks[i].stage = Initialized;

		//Queue task
		poolThread.addWork((Task*)&sliceTasks[i]);
	}

	//wait all task finish
	int k = 0;
	bool frameDone = false;
	while (frameDone == false){

		if (k + 1 == (threadCount * 2)) {
			frameDone = true;
		}

		if (sliceTasks[k].stage == Finished) {
			k++;
		}

	}

}









//#include "SDL.h"
//#include "Windows.h"
//#include <vector>
//#include <thread>
//
//
//int width = 800;
//int height = 800;
//
//
//long double min = -2.0;
//long double max = 2.0;
//
//long double factor = 1;
//
//int MAX_ITERATIONS = 200;
//
//
//long double map(long double value, long double in_min, long double in_max, long double out_min, long double out_max) {
//
//	return (value - in_min) *(out_max - out_min) / (in_max - in_min) + out_min;
//}
//
//int main(int argc, char* argv[]) {
//
//
//	SDL_Init(SDL_INIT_EVERYTHING);
//
//	SDL_Window* window;
//	SDL_Renderer* renderer;
//	SDL_Event event;
//
//
//	SDL_CreateWindowAndRenderer(1440, 900, 0, &window, &renderer);
//	SDL_RenderSetLogicalSize(renderer, width, height);
//
//	int count = 0;
//
//	while (1) {
//
//		//max -= 0.1*factor;
//		//min += 0.15 * factor;
//		//factor *= 0.9349;
//		//MAX_ITERATIONS += 5;
//
//		//if (count > 30) {
//		//	MAX_ITERATIONS *= 1.02;
//		//}
//	
//		SDL_RenderPresent(renderer);
//
//		for (int x = 0; x < width; x++) {//x = a
//			
//			if (SDL_PollEvent(&event) && event.type == SDL_QUIT) {
//				return 0;
//			}
//
//			if (GetKeyState('Q') & 0x8000) {
//				return 0;
//			}
//
//			for (int y = 0; y < height; y++) {//y = b
//				
//				long double a = map(x, 0, width, min, max);
//				long double b = map(y, 0, height, min, max);
//			
//				long double aIni = a;
//				long double bIni = b;
//
//				int n = 0;
//
//				for (int i = 0; i < MAX_ITERATIONS; i++) {
//				
//					long double a1 = a * a - b * b;
//					long double b1 = 2 * a *b;
//
//					a = a1 + aIni;
//					b = b1 + bIni;
//
//					if ((a + b) > 2) {
//						break;
//					}
//
//					n++;
//				}
//
//				int bright = map(n, 0 , MAX_ITERATIONS, 0, 255);
//
//				if ((n == MAX_ITERATIONS) || (bright < 20)) {
//					bright = 0;
//				}
//
//				int red = map(bright*bright, 0, 255*255, 0, 255);
//				int green = bright;
//				int blue = map(sqrt(bright), 0, sqrt(255), 0, 255);
//
//				SDL_SetRenderDrawColor(renderer, red, green, blue, 255);
//				SDL_RenderDrawPoint(renderer, x, y);
//			}
//		
//		}
//	/*	count+= 20;*/
//	}
//
//	return 0;
//}
//
