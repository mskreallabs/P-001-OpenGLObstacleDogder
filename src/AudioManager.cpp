#include "AudioManager.h"
#include <iostream>
#include <fstream>
#include <vector>

AudioManager& AudioManager::GetInstance() {
    static AudioManager instance;
    return instance;
}

bool AudioManager::Initialize() {
    // Open audio device
    m_Device = alcOpenDevice(nullptr);
    if (!m_Device) {
        std::cerr << "ERROR: Failed to open audio device!" << std::endl;
        return false;
    }

    // Create context
    m_Context = alcCreateContext(m_Device, nullptr);
    if (!m_Context) {
        std::cerr << "ERROR: Failed to create audio context!" << std::endl;
        alcCloseDevice(m_Device);
        return false;
    }

    // Make context current
    if (!alcMakeContextCurrent(m_Context)) {
        std::cerr << "ERROR: Failed to make context current!" << std::endl;
        alcDestroyContext(m_Context);
        alcCloseDevice(m_Device);
        return false;
    }

    std::cout << "Audio System Initialized Successfully!" << std::endl;
    return true;
}

void AudioManager::Shutdown() {
    StopAll();

    // Delete all sources and buffers
    for (auto& sound : m_Sounds) {
        if (sound.second.source) alDeleteSources(1, &sound.second.source);
        if (sound.second.buffer) alDeleteBuffers(1, &sound.second.buffer);
    }
    m_Sounds.clear();

    // Clean up OpenAL
    if (m_Context) {
        alcMakeContextCurrent(nullptr);
        alcDestroyContext(m_Context);
        m_Context = nullptr;
    }
    if (m_Device) {
        alcCloseDevice(m_Device);
        m_Device = nullptr;
    }

    std::cout << "Audio System Shutdown." << std::endl;
}

bool AudioManager::LoadWAV(const std::string& filepath, std::vector<char>& data,
    ALenum& format, ALsizei& freq) {
    struct WAVHeader {
        char riff[4];
        unsigned int chunkSize;
        char wave[4];
        char fmt[4];
        unsigned int subchunk1Size;
        unsigned short audioFormat;
        unsigned short numChannels;
        unsigned int sampleRate;
        unsigned int byteRate;
        unsigned short blockAlign;
        unsigned short bitsPerSample;
        char data[4];
        unsigned int dataSize;
    };

    std::ifstream file(filepath, std::ios::binary);
    if (!file.is_open()) {
        std::cerr << "ERROR: Cannot open sound file: " << filepath << std::endl;
        return false;
    }

    WAVHeader header;
    file.read(reinterpret_cast<char*>(&header), sizeof(WAVHeader));

    if (header.audioFormat != 1) {
        std::cerr << "ERROR: Only PCM WAV format supported!" << std::endl;
        return false;
    }

    if (header.numChannels == 1) {
        format = (header.bitsPerSample == 8) ? AL_FORMAT_MONO8 : AL_FORMAT_MONO16;
    }
    else if (header.numChannels == 2) {
        format = (header.bitsPerSample == 8) ? AL_FORMAT_STEREO8 : AL_FORMAT_STEREO16;
    }
    else {
        std::cerr << "ERROR: Unsupported number of channels!" << std::endl;
        return false;
    }

    freq = header.sampleRate;

    data.resize(header.dataSize);
    file.read(data.data(), header.dataSize);
    file.close();

    return true;
}

bool AudioManager::LoadSound(const std::string& name, const std::string& filepath) {
    if (m_Sounds.find(name) != m_Sounds.end()) {
        std::cout << "Sound already loaded: " << name << std::endl;
        return true;
    }

    std::vector<char> data;
    ALenum format;
    ALsizei freq;

    if (!LoadWAV(filepath, data, format, freq)) {
        return false;
    }

    ALuint buffer;
    alGenBuffers(1, &buffer);
    alBufferData(buffer, format, data.data(), data.size(), freq);

    ALuint source;
    alGenSources(1, &source);
    alSourcei(source, AL_BUFFER, buffer);
    alSourcef(source, AL_PITCH, 1.0f);
    alSourcef(source, AL_GAIN, 1.0f);
    alSourcei(source, AL_LOOPING, AL_FALSE);

    Sound sound;
    sound.buffer = buffer;
    sound.source = source;
    sound.isMusic = false;
    m_Sounds[name] = sound;

    std::cout << "Loaded sound: " << name << std::endl;
    return true;
}

void AudioManager::Play(const std::string& name, float volume) {
    auto it = m_Sounds.find(name);
    if (it == m_Sounds.end()) return;

    ALuint source = it->second.source;
    alSourceStop(source);
    alSourcei(source, AL_LOOPING, AL_FALSE);
    alSourcef(source, AL_GAIN, volume);
    alSourcePlay(source);
}

void AudioManager::PlayMusic(const std::string& name, float volume) {
    auto it = m_Sounds.find(name);
    if (it == m_Sounds.end()) return;

    StopMusic();

    it->second.isMusic = true;
    m_CurrentMusic = name;

    ALuint source = it->second.source;
    alSourceStop(source);
    alSourcei(source, AL_LOOPING, AL_TRUE);
    alSourcef(source, AL_GAIN, volume);
    alSourcePlay(source);
}

void AudioManager::Stop(const std::string& name) {
    auto it = m_Sounds.find(name);
    if (it != m_Sounds.end()) {
        alSourceStop(it->second.source);
    }
}

void AudioManager::StopMusic() {
    if (!m_CurrentMusic.empty()) {
        auto it = m_Sounds.find(m_CurrentMusic);
        if (it != m_Sounds.end()) {
            alSourceStop(it->second.source);
            it->second.isMusic = false;
        }
        m_CurrentMusic.clear();
    }
}

void AudioManager::StopAll() {
    for (auto& sound : m_Sounds) {
        alSourceStop(sound.second.source);
    }
}

void AudioManager::SetMusicVolume(float volume) {
    if (!m_CurrentMusic.empty()) {
        auto it = m_Sounds.find(m_CurrentMusic);
        if (it != m_Sounds.end()) {
            alSourcef(it->second.source, AL_GAIN, volume);
        }
    }
}

bool AudioManager::IsPlaying(const std::string& name) {
    auto it = m_Sounds.find(name);
    if (it == m_Sounds.end()) return false;

    ALint state;
    alGetSourcei(it->second.source, AL_SOURCE_STATE, &state);
    return (state == AL_PLAYING);
}