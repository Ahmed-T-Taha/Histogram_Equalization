#include <iostream>
#include <math.h>
#include <stdlib.h>
#include <string.h>
#include <msclr\marshal_cppstd.h>
#include <ctime>
#include <filesystem>
#include <mpi.h>

#using <mscorlib.dll>
#using <System.dll>
#using <System.Drawing.dll>
#using <System.Windows.Forms.dll>

using namespace msclr::interop;
using std::cout;
using std::endl;
using std::filesystem::directory_iterator;
using std::filesystem::current_path;
using std::filesystem::path;
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

	path outputPath = current_path().parent_path().parent_path() / "Data" / "Output" / "MPI" / imageName;
	System::String^ imagePath =
		marshal_as<System::String^>(outputPath.string());
	MyNewImage.Save(imagePath);
}


int main()
{
	MPI_Init(NULL, NULL);
	int rank, size;
	MPI_Comm_size(MPI_COMM_WORLD, &size);
	MPI_Comm_rank(MPI_COMM_WORLD, &rank);

	path inputPath = current_path().parent_path().parent_path() / "Data" / "Input";

	for (const auto& entry : directory_iterator(inputPath))
	{
		auto ext = entry.path().extension().string();
		if ((ext != ".jpeg") && (ext != ".jpg") && (ext != ".png"))
			continue;
		std::string imageName = entry.path().filename().string();


		int* intensityMapping = new int[256],
			* imageData = nullptr,
			* localPixelCounts = new int[256] {0},
			* pixelCounts = nullptr;
		int imageWidth, imageHeight, totalPixels;

		if (rank == 0)
		{
			cout << "\nProcessing image " << imageName << endl;
			System::String^ imagePath = marshal_as<System::String^>(entry.path().string());
			imageData = inputImage(&imageWidth, &imageHeight, imagePath);
			cout << "Read image " << imageName << endl;
			totalPixels = imageWidth * imageHeight;
			pixelCounts = new int[256];
		}

		int start_s = clock();
		// Start of measured region


		// Broadcast the total number of pixels to all processes
		MPI_Bcast(&totalPixels, 1, MPI_INT, 0, MPI_COMM_WORLD);

		// Distribute the image data to all processes
		int* localImageData = new int[totalPixels / size];
		MPI_Scatter(imageData, totalPixels / size, MPI_INT, localImageData, totalPixels / size, MPI_INT, 0, MPI_COMM_WORLD);

		// Calculate the occurrence of each pixel value in the image
		for (int i = 0; i < totalPixels / size; i++)
			localPixelCounts[localImageData[i]]++;

		MPI_Reduce(localPixelCounts, pixelCounts, 256, MPI_INT, MPI_SUM, 0, MPI_COMM_WORLD);

		// Get the cumulative probabilities multiplied by 255 for each pixel value
		if (rank == 0)
		{
			int cumulativeSum = 0;
			for (int i = 0; i < 256; i++)
			{
				cumulativeSum += pixelCounts[i];
				intensityMapping[i] = (double)cumulativeSum / totalPixels * 255;
			}
		}
		MPI_Bcast(intensityMapping, 256, MPI_INT, 0, MPI_COMM_WORLD);

		// Get the new pixel value for each pixel in the image
		for (int i = 0; i < totalPixels / size; i++)
			localImageData[i] = intensityMapping[localImageData[i]];

		MPI_Gather(localImageData, totalPixels / size, MPI_INT, imageData, totalPixels / size, MPI_INT, 0, MPI_COMM_WORLD);


		// End of measured region
		int stop_s = clock();

		if (rank == 0)
		{
			double timeTaken = (stop_s - start_s) / double(CLOCKS_PER_SEC) * 1000;
			cout << "Time taken to process image " << imageName << ": " << timeTaken << "ms" << endl;
			createImage(imageData, imageWidth, imageHeight, imageName);
			cout << "Image saved as " << imageName << endl;

			delete[] imageData;
			delete[] pixelCounts;
		}

		delete[] localImageData;
		delete[] localPixelCounts;
		delete[] intensityMapping;
	}

	MPI_Finalize();
	return 0;
}
