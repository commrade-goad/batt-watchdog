#include <cstdio>
#include <iostream>
#include <fstream>
#include <sstream>
#include <cstdlib>
#include <string>
#include <vector>
#include <csignal>
#include <unistd.h>
#include <SDL2/SDL_mixer.h>
#include <SDL2/SDL.h>
using namespace std;

#define USE_LOCK_MANAGEMENT
#define USE_STARTUP_SOUND

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

typedef enum {
    Charging,
    Discharging,
    Full,
    Unknown,
} BattStatus;

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

BattStatus readStatus() {
    ifstream status_path(STATUS);
    string status;
    if (!status_path.good()) return BattStatus::Unknown;
    getline (status_path, status);
    status_path.close();
    if (status == "Discharging") return BattStatus::Discharging;
    else if (status == "Charging") return BattStatus::Charging;
    else if (status == "Full") return BattStatus::Full;
    else return BattStatus::Unknown;
}

int playAudio(string path_to_file) {
    ifstream file(path_to_file);
    if (!file.good()) return 1;
    if (SDL_Init(SDL_INIT_AUDIO) < 0)
    {
        cerr << "ERROR : Failed to initialize SDL audio system: " << SDL_GetError() << endl;
        return 1;
    }
    if (Mix_OpenAudio(AUDIO_FREQUENCY, MIX_DEFAULT_FORMAT, AUDIO_CHANNELS, AUDIO_CHUNKSIZE) < 0) 
    {
        cerr << "ERROR : Failed to open audio device: " << Mix_GetError() << endl;
        return 1;
    }
    string audioExt = path_to_file.substr(path_to_file.find_last_of('.'));
    if (audioExt == ".mp3" || audioExt == ".wav") {
        Mix_Chunk * audio = Mix_LoadWAV(path_to_file.c_str());
        if (!audio) {
            cerr << "ERROR: Failed to load audio file: " << Mix_GetError() << endl;
            Mix_CloseAudio();
            SDL_Quit();
            return 1;
        }
        int channel = Mix_PlayChannel(-1, audio, 0);
        if (channel == -1) {
            cerr << "ERROR: Failed to play audio file: " << Mix_GetError() << endl;
        }
        while (Mix_Playing(channel) != 0) {
            SDL_Delay(300);
        }

        Mix_CloseAudio();
        SDL_Quit();
    } else {
        cerr << "ERROR: Unsupported audio format " << audioExt << endl;
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
            cerr << "ERROR : Failed to execute command: " << command << endl;
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
        cerr << "ERROR : Program is already running" << endl;
        return 1;
    }
}

void sigHandler(int signal)
{
    int result = remove(LOCK_PATH.c_str());
    if (result == 0)
        {
            cout << "\nProgram Terminated successfully" << endl;
        }
    else
    {
        cerr << "\nERROR: Failed to remove the lock file" << endl;
    }
    exit(signal);
}

// void atexitHandler()
// {
//     int result = remove(LOCK_PATH.c_str());
//     if (result != 0) cout << "ERROR: Failed to remove the lock file" << endl;
//     exit(1);
// }

int main()
{
#ifdef USE_LOCK_MANAGEMENT
    if (lockFileManagement() != 0) exit(1);
    // SIG HANDLE
    signal(SIGTERM, sigHandler);
    signal(SIGINT, sigHandler);
    // atexit(atexitHandler);
#endif
#ifdef USE_STARTUP_SOUND
    if (playAudio(AUDIO_PATH) != 0) cerr << "ERROR : Failed to play audio." << endl;
#endif
    bool running = true;
    while (running == true)
    {
        BattStatus battStatus = readStatus();
        int battPercentage = readPercentage();
        cout << battPercentage << endl;
        switch (battStatus) {
            case BattStatus::Discharging:
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
                            if (playAudio(AUDIO_PATH) != 0) cerr << "ERROR : Failed to play audio." << endl;
                            sleep(SLEEP_TIME_NORMAL);
                            break;
                        default:
                            break;
                    }
                    case BattStatus::Charging:
                        {
                            cout << "Battery currently Charging" << endl;
                            sleep(SLEEP_TIME_LONG);
                        }
                    case BattStatus::Full:
                        {
                            cout << "Battery currently Full!" << endl;
                            sleep(SLEEP_TIME_LONG);
                        } 
                    default:
                        {
                            cout << "Battery currently Unknown!" << endl;
                            sleep(SLEEP_TIME_LONG);
                        }
                }
        }
    }
    return 0;
}
