#pragma once
#ifndef AUDIO_MANAGER_H
#define AUDIO_MANAGER_H

#include <string>
#include <map>
#include <vector>
#include <AL/al.h>
#include <AL/alc.h>

class AudioManager {
public:
    static AudioManager& GetInstance();

    bool Initialize();
    void Shutdown();

    // Load WAV sound file
    bool LoadSound(const std::string& name, const std::string& filepath);

    // Play sound effects
    void Play(const std::string& name, float volume = 1.0f);

    // Play background music (looping)
    void PlayMusic(const std::string& name, float volume = 0.5f);

    // Stop sounds
    void Stop(const std::string& name);
    void StopMusic();
    void StopAll();

    // Volume control
    void SetMusicVolume(float volume);

    // Check if sound is playing
    bool IsPlaying(const std::string& name);

private:
    AudioManager() = default;
    ~AudioManager() = default;

    struct Sound {
        ALuint buffer = 0;
        ALuint source = 0;
        bool isMusic = false;
    };

    ALCdevice* m_Device = nullptr;
    ALCcontext* m_Context = nullptr;
    std::map<std::string, Sound> m_Sounds;
    std::string m_CurrentMusic;

    // WAV loader helper
    bool LoadWAV(const std::string& filepath, std::vector<char>& data,
        ALenum& format, ALsizei& freq);
};

#endif