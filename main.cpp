#include <stdio.h>
#include <stdlib.h>
#include <string>
#include <cstring>

#include <lame.h>
#include <pthread.h>
#include <dirent.h>

#ifndef WIN32
#include <unistd.h>
#endif

// encodes a single wav file to mp3, called concurrently
// wavFilePath - input wav file path
// mp3FilePath - output mp3 file path
// mutex - mutex used for locking standard output stream
int encodeWav2Mp3(std::string wavFilePath, std::string mp3FilePath, pthread_mutex_t* mutex);

void* threadFunction(void* arguments);

// structure for transfering data into thread
struct SThreadData
{
  std::string wavFilePath;
  std::string mp3FilePath;
  int* runningThreadCount;
  pthread_mutex_t* mutex;
};

// unified sleep function
void wait(int seconds);

int main(int argc, char* argv[])
{
  // chack command-line arguments
  if (argc != 2)
  {
    printf("Wrong number of arguments. Usage: wav2mp3 <wav_files_path>\n");
    return 1;
  }

  // using dirent to scan directory
  DIR* directory = opendir(argv[1]);
  if (!directory)
  {
    printf("Error opening directory: %s\n", argv[1]);
    return 2;
  }

  // get total number of cpu cores, used as max number of running threads
#ifdef WIN32
  SYSTEM_INFO sysinfo;
  GetSystemInfo(&sysinfo);
  long numberOfCPUCores = sysinfo.dwNumberOfProcessors;
#else
  long numberOfCPUCores = sysconf(_SC_NPROCESSORS_ONLN);
#endif

  printf("Number of CPU cores: %ld\n", numberOfCPUCores);

  // mutex to lock standard output stream
  pthread_mutex_t mutex;
  pthread_mutex_init(&mutex, 0);

  std::string wavDir = argv[1];
  // ensure the presense of separator at the end of the path
#ifdef WIN32
  if (wavDir[wavDir.length() - 1] != '\\')
    wavDir.append("\\");
#else
  if (wavDir[wavDir.length() - 1] != '/')
    wavDir.append("/");
#endif

  // number of currently running threads
  int runningThreadCount = 0;

  // scan the input directory for wav files and encode
  dirent* file;
  while ((file = readdir(directory)) != 0)
  {
    // check extension, skip if not *.wav file
    char* ext = strrchr(file->d_name, '.');
    if (!ext || strcmp(ext, ".wav") != 0)
    {
      printf("\n%s is not a wav file, skipped", file->d_name);
      continue;
    }

    printf("\n"); // formatting
    while (runningThreadCount >= numberOfCPUCores)
    {
      printf("\rReached maximum number of running threads: %d, sleep", runningThreadCount);
      wait(1);
    }

    // preparing input parameters
    std::string wavFilePath = wavDir;
    wavFilePath.append(file->d_name);
    std::string mp3FilePath = wavFilePath;
    // change extension, replace trailing ".wav" with ".MP3"
    mp3FilePath.replace(mp3FilePath.length() - 4, 4, ".MP3");

    // thread arguments
    SThreadData* arguments = new SThreadData;
    arguments->wavFilePath = wavFilePath;
    arguments->mp3FilePath = mp3FilePath;
    arguments->runningThreadCount = &runningThreadCount;
    arguments->mutex = &mutex;

    // create a new thread and run encoder
    pthread_t encodingThread;
    if (pthread_create(&encodingThread, NULL, threadFunction, arguments) != 0)
    {
      printf("\nError creating thread for %s", wavFilePath.c_str());
      pthread_cancel(encodingThread);
    }
    else
    {
      // thread created succesfully, incrementing the number of running threads, synchronized
      if (pthread_mutex_lock(&mutex) == 0)
      {
        ++runningThreadCount;
        pthread_mutex_unlock(&mutex);
      }

      if (pthread_detach(encodingThread) != 0)
      {
        printf("\nError detaching thread for %s from main", wavFilePath.c_str());
      }
    }
  }
  printf("\n"); // formatting
  // wait for running theads to finish
  while (runningThreadCount > 0)
  {
    printf("\rWaiting for threads to finish. Number of running threads: %d ", runningThreadCount);
    wait(1);
  }
  printf("\nAll threads finished.\n");

  pthread_mutex_destroy(&mutex);

  return 0;
}

int encodeWav2Mp3(std::string wavFilePath, std::string mp3FilePath, pthread_mutex_t* mutex)
{
  FILE* wavFile = fopen(wavFilePath.c_str(), "rb");
  if (!wavFile)
  {
    if (pthread_mutex_lock(mutex) == 0)
    {
      printf("Error opening wav file for reading: %s\n", wavFilePath.c_str());
      pthread_mutex_unlock(mutex);
    }
    return 1;
  }

  FILE* mp3File = fopen(mp3FilePath.c_str(), "wb");
  if (!mp3File)
  {
    if (pthread_mutex_lock(mutex) == 0)
    {
      printf("Error opening mp3 file for writing: %s\n", mp3FilePath.c_str());
      pthread_mutex_unlock(mutex);
    }
    return 2;
  }

  // initialize the lame encoder
  lame_global_flags* lameFlags = lame_init();
  // check for lam error (unable to malloc)
  if (!lameFlags)
  {
    if (pthread_mutex_lock(mutex) == 0)
    {
      printf("Error initializing lame encoder\n");
      pthread_mutex_unlock(mutex);
    }
    return 3;
  }

  // set lame quality - good
  lame_set_quality(lameFlags, 5);
  if (lame_init_params(lameFlags) < 0)
  {
    if (pthread_mutex_lock(mutex) == 0)
    {
      printf("Error setting lame parameters\n");
      pthread_mutex_unlock(mutex);
    }
    return 4;
  }

  if (pthread_mutex_lock(mutex) == 0)
  {
    printf("\nConvertion started: %s\n", wavFilePath.c_str());
    pthread_mutex_unlock(mutex);
  }

  // skip wav header, to eliminate a click sound at the beginning of mp3 file
  fseek(wavFile, 4096, 0);

  // file is read by chunks, using first power-of-two after 7200 (needed for lame)
  const int ReadBufferSize = 8192;
  // read/write buffers
  short* wavBuffer = new short[2 * ReadBufferSize];
  unsigned char* mp3Buffer = new unsigned char[ReadBufferSize];

  // read wav file, encode, write into mp3 file
  while (size_t readSize = fread(wavBuffer, 2 * sizeof(short), ReadBufferSize, wavFile))
  {
    // split input stream into two channels
    short* leftChannel = new short[readSize];
    short* rightChannel = new short[readSize];
    int currIndex = 0;
    for (size_t i = 0; i < readSize; i++)
    {
      leftChannel[i] = wavBuffer[currIndex++];
      rightChannel[i] = wavBuffer[currIndex++];
    }

    int writeSize = lame_encode_buffer(lameFlags, leftChannel, rightChannel, readSize, mp3Buffer, ReadBufferSize);

    // free allocated memory
    delete[] leftChannel;
    delete[] rightChannel;
    // check for lame errors
    if (writeSize < 0)
    {
      if (pthread_mutex_lock(mutex) == 0)
      {
        printf("\nError during encoding, writeSize: %d", writeSize);
        pthread_mutex_unlock(mutex);
      }
      return 5;
    }
    // write mp3 buffer to mp3 file
    fwrite(mp3Buffer, writeSize, 1, mp3File);
  }

  // complete the final frame, _nogap is to eliminate a click sound at the end of mp3 file
  int flushSize = lame_encode_flush_nogap(lameFlags, mp3Buffer, ReadBufferSize);
  // write last flushed buffer, if any
  fwrite(mp3Buffer, flushSize, 1, mp3File);

  // close lame flags
  lame_close(lameFlags);

  // free allocated memory
  delete[] wavBuffer;
  delete[] mp3Buffer;

  // close files
  fclose(wavFile);
  fclose(mp3File);

  if (pthread_mutex_lock(mutex) == 0)
  {
    printf("\nConvertion completed: %s\n", mp3FilePath.c_str());
    pthread_mutex_unlock(mutex);
  }

  return 0;
}

void wait(int seconds)
{
#ifdef WIN32
  Sleep(seconds * 1000);
#else
  sleep(seconds);
#endif
}

// encoding thread function
void* threadFunction(void* arguments)
{
  SThreadData* threadData = ((SThreadData*)(arguments));
  if (encodeWav2Mp3(threadData->wavFilePath, threadData->mp3FilePath, threadData->mutex))
  {
    printf("Error converting wav file: %s\n", threadData->wavFilePath.c_str());
  }

  // decrement the number of running threads, synchronized
  if (pthread_mutex_lock(threadData->mutex) == 0)
  {
    --(*threadData->runningThreadCount);
    pthread_mutex_unlock(threadData->mutex);
  }

  delete threadData;
  return 0;
}

