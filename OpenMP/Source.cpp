#include <iostream>
#include <math.h>
#include <stdlib.h>
#include <string.h>
#include <msclr\marshal_cppstd.h>
#include <ctime>
#include <filesystem>
#include <omp.h>

#using <mscorlib.dll>
#using <System.dll>
#using <System.Drawing.dll>
#using <System.Windows.Forms.dll>

using namespace msclr::interop;
using std::cout;
using std::endl;
using std::filesystem::directory_iterator;
using System::Drawing::Color;
using System::Drawing::Bitmap;


// Read Image and save it to local arrays
// Put the size of image in w & h
int* inputImage(int* w, int* h, System::String^ imagePath)
{
	Bitmap BM(imagePath);
	*w = BM.Width;
	*h = BM.Height;
	int* input = new int[BM.Height * BM.Width];

	for (int i = 0; i < BM.Height; i++)
	{
		for (int j = 0; j < BM.Width; j++)
		{
			//gray scale value equals the average of RGB values
			Color c = BM.GetPixel(j, i);
			input[i * BM.Width + j] = (c.R + c.B + c.G) / 3;
		}
	}
	return input;
}


void createImage(int* image, int width, int height, std::string imageName)
{
	System::Drawing::Bitmap MyNewImage(width, height);

	for (int i = 0; i < MyNewImage.Height; i++)
	{
		for (int j = 0; j < MyNewImage.Width; j++)
		{
			int pixelLoc = i * width + j;
			if (image[pixelLoc] < 0)
				image[pixelLoc] = 0;
			else if (image[pixelLoc] > 255)
				image[pixelLoc] = 255;
			int newPixelLoc = i * MyNewImage.Width + j;
			Color c = Color::FromArgb(image[newPixelLoc], image[newPixelLoc], image[newPixelLoc]);
			MyNewImage.SetPixel(j, i, c);
		}
	}

	System::String^ imagePath =
		marshal_as<System::String^>("..//Data//Output//OpenMP//" + imageName);
	MyNewImage.Save(imagePath);
}


int main()
{
	cout << "This is the OpenMP program."
		<< "\nEnsure all images are in the \"Data/Input\" directory"
		<< "\nThen press enter to equalize their histograms...";
	getchar();

	for (const auto& entry : directory_iterator("..//Data//Input"))
	{
		auto ext = entry.path().extension().string();
		if ((ext != ".jpeg") && (ext != ".jpg") && (ext != ".png"))
			continue;

		std::string imageName = entry.path().filename().string();
		cout << "\nProcessing image " << imageName << endl;
		System::String^ imagePath = marshal_as<System::String^>(entry.path().string());
		int imageWidth, imageHeight;
		int* imageData = inputImage(&imageWidth, &imageHeight, imagePath);
		int totalPixels = imageWidth * imageHeight;
		cout << "Read image " << imageName << endl;

		int start_s = clock();
		// Start of measured region


		int pixelCounts[256] = { 0 };
		int intensityMapping[256];
		int cumulativeSums[256];

#pragma omp parallel
		{
			// Calculate the occurrence of each pixel value in the image
			int localPixelCounts[256] = { 0 };
#pragma omp for nowait
			for (int i = 0; i < totalPixels; i++)
				localPixelCounts[imageData[i]]++;
#pragma omp critical
			{
				for (int i = 0; i < 256; i++)
					pixelCounts[i] += localPixelCounts[i];
			}
#pragma omp barrier

			// Get the cumulative probabilities multiplied by 255 for each pixel value
#pragma omp single
			{
				cumulativeSums[0] = pixelCounts[0];
				for (int i = 1; i < 256; i++)
					cumulativeSums[i] = pixelCounts[i] + cumulativeSums[i - 1];
			}
#pragma omp for
			for (int i = 0; i < 256; i++)
				intensityMapping[i] = (double)cumulativeSums[i] / totalPixels * 255;

			// Get the new pixel value for each pixel in the image
#pragma omp for
			for (int i = 0; i < totalPixels; i++)
				imageData[i] = intensityMapping[imageData[i]];
		}


		// End of measured region
		int stop_s = clock();

		double timeTaken = (stop_s - start_s) / double(CLOCKS_PER_SEC) * 1000;
		cout << "Time taken to process image " << imageName << ": " << timeTaken << "ms" << endl;
		createImage(imageData, imageWidth, imageHeight, imageName);
		cout << "Image saved as " << imageName << endl;

		delete[] imageData;
	}
	return 0;
}

/*
Processing Times:
- Clouds: 70ms
- Desert: 6ms
- Einstein: 4ms
- Ocean: 16ms
- Sparrows: 3ms
*/