#include <cstdio>
#include <iostream>
#include <fstream>
#include <sstream>
#include <cstdlib>
#include <stdexcept>
#include <string>
#include <vector>
#include <csignal>
#include <unistd.h>
#include <SDL2/SDL_mixer.h>
#include <SDL2/SDL.h>
using namespace std;

// PATH
const string STATUS = "/sys/class/power_supply/BAT1/status";
const string PERCENTAGE = "/sys/class/power_supply/BAT1/capacity";
const string AUDIO_PATH = "/home/fernando/git/rust-batt-reminder/assets/notification_sound.mp3";
const string LOCK_PATH = "/tmp/batt-watchdog.lock";

// Sleeping time configuration
const int SLEEP_TIME_LONG = 300;
const int SLEEP_TIME_NORMAL = 120;
const int SLEEP_TIME_FAST = 5;

// Battery configuration
const int BATT_CRITICAL = 30;
const int BATT_LOW = 45;

// AUDIO SETUP
const int AUDIO_FREQUENCY = 44100;
const int AUDIO_CHANNELS = 2;
const int AUDIO_CHUNKSIZE = 1024;

// MESSAGE
const string MESSAGE = "\"<percentage>% Battery Remaining. Please plug in the charger.\"";

bool at_exit = false;

int readPercentage()
{
    ifstream percentage_path(PERCENTAGE);
    string percentage;
    int result;
    stringstream ss;
    if (!percentage_path.good()) return -1;
    getline (percentage_path, percentage);
    percentage_path.close();
    ss << percentage;
    ss >> result;
    return result;
}

string readStatus()
{
    ifstream status_path(STATUS);
    string status;
    if (!status_path.good()) return "\0";
    getline (status_path, status);
    status_path.close();
    return status;
}

int playAudio(string path_to_file) {
    ifstream file(path_to_file);
    if (!file.good()) return 1;
    if (SDL_Init(SDL_INIT_AUDIO) < 0)
    {
        cout << "ERROR : Failed to initialize SDL audio system: " << SDL_GetError() << endl;
        return 1;
    }
    if (Mix_OpenAudio(AUDIO_FREQUENCY, MIX_DEFAULT_FORMAT, AUDIO_CHANNELS, AUDIO_CHUNKSIZE) < 0) 
    {
        cout << "ERROR : Failed to open audio device: " << Mix_GetError() << endl;
        return 1;
    }
    string audioExt = path_to_file.substr(path_to_file.find_last_of('.'));
    if (audioExt == ".mp3")
    {
        Mix_Music* audio = Mix_LoadMUS(path_to_file.c_str());
        if (!audio)
        {
            cout << "ERROR: Failed to load audio file: " << Mix_GetError() << endl;
            return 1;
        }
        Mix_PlayMusic(audio, 0);
        while (Mix_PlayingMusic())
        {
            SDL_Delay(100);
        }
        Mix_FreeMusic(audio);
    } 
    else if (audioExt == ".wav")
    {
        Mix_Chunk* audio = Mix_LoadWAV(path_to_file.c_str());
        if (!audio)
        {
            cout << "ERROR: Failed to load audio file: " << Mix_GetError() << endl;
            return 1;
        }
        Mix_PlayChannel(-1, audio, 0);
        while (Mix_Playing(-1))
        {
            SDL_Delay(100);
        }
        Mix_FreeChunk(audio);
    }
    else
    {
        cout << "ERROR: Unsupported audio format " << audioExt << endl;
        return 1;
    }

    Mix_CloseAudio();
    SDL_Quit();

    return 0;
}

int spawnProcess(const vector<string>& args)
{
    string command;

    for (const string& arg : args)
    {
        command += arg + " ";
    }
    int exit_code = system(command.c_str());

    if (exit_code == -1)
    {
        cerr << "Failed to execute command: " << command << endl;
    }
    else
    {
        cout << "command executed!" << endl;
        if (WIFEXITED(exit_code))
        {
            int status = WEXITSTATUS(exit_code);
            cout << "Child process exited with status " << status << endl;
        }
        else if (WIFSIGNALED(exit_code))
        {
            int signal = WTERMSIG(exit_code);
            cout << "Child process terminated with signal " << signal << endl;
        }
    }

    return exit_code;
}

string replaceKey(string message, string from, string to)
{
    string formatted_message = message;
    string percentage_placeholder = from;
    size_t pos = formatted_message.find(percentage_placeholder);
    if (pos != string::npos)
    {
        formatted_message.replace(pos, percentage_placeholder.length(), to);
    }

    return formatted_message;
}

int lockFileManagement()
{
    ifstream lock_file_status(LOCK_PATH);
    if (!lock_file_status.good())
    {
        ofstream lock_file;
        lock_file.open(LOCK_PATH);
        lock_file << "RUNNING";
        lock_file.close();
        return 0;
    }
    else
    {
        cout << "ERROR : Program is already running" << endl;
        return 1;
    }
}

void cleanUp()
{
    at_exit = true;
    int result = remove(LOCK_PATH.c_str());
    if (result != 0) cout << "ERROR: Failed to remove the lock file" << endl;
}

void sigHandler(int signal)
{
    if (!at_exit)
    {
        cleanUp();
        exit(signal);
    }
}


int main()
{
    // SIG HANDLE
    signal(SIGTERM, sigHandler);
    signal(SIGINT, sigHandler);
    atexit(cleanUp);
    //
    if (playAudio(AUDIO_PATH) != 0) cout << "ERROR : Failed to play audio." << endl;
    if (lockFileManagement() != 0) exit(1);
    bool running = true;
    while (running == true)
    {
        string battStatus = readStatus();
        int battPercentage = readPercentage();
        cout << battPercentage << endl;
        if (battStatus == "Discharging")
        {
            cout << "Battery currently Discharging" << endl;
            switch (battPercentage) {
                case BATT_LOW ... 100:
                    cout << "sleeping for : " << SLEEP_TIME_LONG << endl;
                    sleep(SLEEP_TIME_LONG);
                    break;
                case BATT_CRITICAL ... BATT_LOW-1:
                    cout << "sleeping for : " << SLEEP_TIME_FAST << endl;
                    sleep(SLEEP_TIME_FAST);
                    break;
                case 1 ... BATT_CRITICAL-1 :
                    cout << "sleeping for : " << SLEEP_TIME_NORMAL << endl;
                    spawnProcess({"/usr/bin/notify-send", "--app-name=Battery", "-u", "critical", "-t", "10000", replaceKey(MESSAGE, "<percentage>", to_string(battPercentage))});
                    if (playAudio(AUDIO_PATH) != 0) cout << "ERROR : Failed to play audio." << endl;
                    sleep(SLEEP_TIME_NORMAL);
                    break;
                default:
                    break;
            }

        } 
        else if (battStatus == "Charging")
        {
            cout << "Battery currently Charging" << endl;
            sleep(SLEEP_TIME_LONG);
        } 
        else if (battStatus == "Full")
        {
            cout << "Battery currently Full!" << endl;
            sleep(SLEEP_TIME_LONG);
        } 
        else
        {
            cout << "Battery currently Unknown!" << endl;
            sleep(SLEEP_TIME_LONG);
        }
    }
    return 0;
}
