/* Read-only CoreAudio output diagnostics; never changes routing or volume. */
#include <CoreAudio/CoreAudio.h>
#include <CoreFoundation/CoreFoundation.h>
#include <stdio.h>

static OSStatus get(AudioObjectID object, AudioObjectPropertySelector selector,
                    AudioObjectPropertyScope scope, AudioObjectPropertyElement element,
                    void *out, UInt32 size)
{
    AudioObjectPropertyAddress address = {selector, scope, element};
    return AudioObjectGetPropertyData(object, &address, 0, NULL, &size, out);
}

int main(void)
{
    AudioDeviceID device = kAudioObjectUnknown;
    OSStatus status = get(kAudioObjectSystemObject,
        kAudioHardwarePropertyDefaultOutputDevice, kAudioObjectPropertyScopeGlobal,
        kAudioObjectPropertyElementMain, &device, sizeof(device));
    if (status || device == kAudioObjectUnknown) {
        fprintf(stderr, "No default output device; status=%d\n", (int)status);
        return 1;
    }
    CFStringRef name = NULL;
    char text[512] = "unknown";
    status = get(device, kAudioObjectPropertyName, kAudioObjectPropertyScopeGlobal,
                 kAudioObjectPropertyElementMain, &name, sizeof(name));
    if (!status && name) {
        CFStringGetCString(name, text, sizeof(text), kCFStringEncodingUTF8);
        CFRelease(name);
    }
    printf("Default output: %s (device %u)\n", text, device);
    for (unsigned channel = 0; channel <= 2; ++channel) {
        UInt32 mute = 0;
        Float32 volume = 0;
        OSStatus mute_status = get(device, kAudioDevicePropertyMute,
            kAudioDevicePropertyScopeOutput, channel, &mute, sizeof(mute));
        OSStatus volume_status = get(device, kAudioDevicePropertyVolumeScalar,
            kAudioDevicePropertyScopeOutput, channel, &volume, sizeof(volume));
        printf("Output element %u: mute=%u (status %d), volume=%.6f (status %d)\n",
               channel, mute, (int)mute_status, volume, (int)volume_status);
    }
    Float64 rate = 0;
    status = get(device, kAudioDevicePropertyNominalSampleRate,
                 kAudioObjectPropertyScopeGlobal, 0, &rate, sizeof(rate));
    printf("Nominal rate %.0f Hz (status %d); unavailable properties have nonzero status\n",
           rate, (int)status);
    return 0;
}
