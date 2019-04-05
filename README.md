![](https://raw.githubusercontent.com/thesmith1/midi_zonifier/master/Assets/img/logo_small.png)
# MIDI Zonifier

## About the MIDI Zonifier
The concept of zones in MIDI keyboards is pretty straightforward: you divide adjacent keys in groups and make them control different instruments (both software and hardware). Nowadays almost all MIDI controllers allow even more than one configuration of zones.

The problem arises when you change the keyboard: all the configurations must be reprogrammed! The MIDI Zonifier removes this need by moving the zones into the software: it doesn't matter which controller you use, you always have your zones ready!

## Dependencies
In order to run the Zonifier you must compile it. The application is written in C++ and based on the JUCE framework.
In addition to that, some dependency library is needed:
 - [Aubio](https://aubio.org/), compiled with support for FFTW3 and double precision.
 - [JSON for Modern C++](https://github.com/nlohmann/json), header-only library, must be copied inside a folder named ExternalLib (sibling of Source).
 
 **Caution:** the application is still in development, many bugs are still to be found.

## Features
- Implement keyboard zones at software level, with any number of (possibly overlapping) zones per configuration
- Usage of multiple simultaneous controllers (as long as they are assigned to different MIDI channels)
- Fast selection of zones configurations by means of MIDI Program Changes
- Zone-wise transpose
- Custom many-to-many mapping of CCs
- Send custom Program Changes when a configuration is selected
- Per-zone, per-note custom harmonization

## Usage
The Zonifier works by reading configurations of zones in JSON files. A basic example is:
``` 
{
    "zones": [
        {
            "inChannel": 1,
            "zones": [
                {
                    "startNote": 0,
                    "endNote": 59,
                    "outChannel": 4,
                    "transpose": 0
                },
                {
                    "startNote": 60,
                    "endNote": 127,
                    "outChannel": 5,
                    "transpose": 12
                }
            ]
        }
    ],
    "programChanges": [],
    "bankSelects": []
}
```

This file assumes only one controller on MIDI channel 1 and divides its keyboard in two zones: the first one will output notes without transpose on channel 4, the second one will output notes on channel 5 with transpose of +12. You are completely free of choosing which notes belong to a zone: zones can overlap and there can be notes that do not belong to any zone (they won't be sent anywhere). Please note that startNote and endNote are included in the zone.

In order to load a configuration on the Zonifier, click on Open Directory and select the folder that contains the file. (**Attention**: the folder you select must contain only JSON files that are configuration files of the Zonifier; any other JSON file and the application will crash!)

If the folder you loaded contains more than one file, you can change the current one by means of the two buttons "Previous File" and "Next File". You can achieve the same also by sending from one of the controllers Program Changes 0 and 1 respectively.

## Beyond the Zones
So far so good, I have my zones and I'm happy, but is that it? Nope! The Zonifier has other features that may or may not result useful (it depends on you), so you can choose to read or to ignore the rest.
### CC Mapping
Keyboard players don't use only the black and white keys to play. Thanks to synthesizers and samplers, there is the possibility to control many other parameters that influence the way the sound is produced or modified, e.g. the cutoff frequency in a filter or the amount of distortion.

Therefore, MIDI controllers have knobs (or sometimes even better, encoders): how to deal with them? The Zonifier has a module that maps any input MIDI CC number to any other CC number: this means that, if your controller's knob outputs CC79 but the cutoff frequency of your awesome filter is controlled by CC36, the Zonifier can translate CC79 in CC36 for you, without MIDI Learn or anything else. Any knob can control any parameter: you just have to tell the Zonifier which is which.

So the Zonifier accepts another kind of JSON file that specifies, for each CC number that your controllers produce, what CC number it has to become (and the output MIDI channel it has to be sent in). This mapping is independent of the zones, which means that, while you change zone configuration, this mapping won't change. Think of the zone configuration as the zones you need to perform a song: as soon as you change the song, you change zone configuration, but the mapping of CCs is always the same, because the controllers and the instruments are the same.

An example of CC mapping file is:
```
{
    "keyboardName": "myAwesomeController",
    "ccMapping": [
        {
            "CConVST": 18,
            "CConKey": 27,
            "outChannel": 1
        },
        {
            "CConVST": 22,
            "CConKey": 6,
            "outChannel": 3
        },
        {
            "CConVST": 13,
            "CConKey": 6,
            "outChannel": 1
        },
        {
            "CConVST": 13,
            "CConKey": 1,
            "outChannel": 3
        },
        {
            "CConVST": 75,
            "CConKey": 80,
            "outChannel": 10
        }
    ]
}
```	
Please note that the mapping is many-to-many, which means that a single knob can now control many parameters and a single parameters can be controlled by many knobs!

The CC mapping file is selected with the button "Open CC Mapping file...". You may be tempted to put the CC Mapping file in the same folder with all the zone configuration files: remember the previous warning or bad things will happen...

### Program Changes
The Zonifier is not just a way of having your zones saved and always there for you, it can also function as the brain that manages your live performance. Sometimes (maybe always), when a new song starts, you need to select a new preset from the infinity of choices that your keyboard offers you: either you need to manually input the preset number, or you need to search it manually (and the other members of the band will start to hate you...) or, even worse, since you have software instruments, you have to touch the mouse... Imagine having to do this for all your instruments, hardware and software!

Luckily, MIDI has a way of controlling which preset we are playing: Program Changes. You send that message and the keyboard sets the current preset as the one defined in the Program Change. Well, good news: the Zonifier also sends Program Changes! You can specify a list of Program Changes in every zones configuration file such that, when that file is loaded, that list of messages is sent (read: when the song changes, you change the zones configuration file and all the right presets in all your instruments will be loaded for you). Let's take the basic example of before and let's add some Program Changes:
``` 
{
    "zones": [
        {
            "inChannel": 1,
            "zones": [
                {
                    "startNote": 0,
                    "endNote": 59,
                    "outChannel": 4,
                    "transpose": 0
                },
                {
                    "startNote": 60,
                    "endNote": 127,
                    "outChannel": 5,
                    "transpose": 12
                }
            ]
        }
    ],
    "programChanges": [
        {
            "outChannel": 6,
            "programChangeNumber": 3
        },
        {
            "outChannel": 7,
            "programChangeNumber": 0
        },
        {
            "outChannel": 16,
            "programChangeNumber": 54
        }
    ],
    "bankSelects": [
        {
            "outChannel": 6,
            "bankNumber": 1
        },
        {
            "outChannel": 16,
            "bankNumber": 0
        }
    ]
}
```

### Harmony
The Zonifier also implements a small function that resembles the Harmonizer you find in voice effect units: you can tell the Zonifier that when you play C4, it has to output C4, E4 and G4 at the same time. You can create chords from a single input. Actually it's more general: given any note, you can specify any set of notes as output! This may seem tedious to do (compared with "play a 3rd in C major", but it offers a lot of possibilities that a Harmonizer doesn't have.

This feature can be used to harmonize (useful for example when playing with pad sounds) but also with arpeggiators: instead of physically playing all the notes that have to be arpeggiated, press one key and the Zonfier will do the rest for you. For example:

```
{
    "zones": [
        {
            "inChannel": 15,
            "zones": [
                {
                    "startNote": 36,
                    "endNote": 51,
                    "outChannel": 6,
                    "transpose": 0,
                    "harmony": [
                        {
                            "inNote": 41,
                            "outNotes": [48, 53, 57, 60, 65, 69]
                        },
                        {
                            "inNote": 43,
                            "outNotes": [50, 55, 58, 62, 67, 70]
                        },
                        {
                            "inNote": 45,
                            "outNotes": [49, 52, 57, 61, 64, 69]
                        },
                        {
                            "inNote": 50,
                            "outNotes": [50, 53, 57, 62, 65, 69]
                        }
                    ]
                }
            ]
        },
        {
            "inChannel": 14,
            "zones": [
                {
                    "startNote": 52,
                    "endNote": 82,
                    "outChannel": 7,
                    "transpose": 0
                },
                {
                    "startNote": 83,
                    "endNote": 83,
                    "outChannel": 9,
                    "transpose": -2
                },
                {
                    "startNote": 84,
                    "endNote": 84,
                    "outChannel": 9,
                    "transpose": -1
                }
            ]
        }
    ],
    "programChanges": [
        {
            "outChannel": 6,
            "programChangeNumber": 2
        },
        {
            "outChannel": 7,
            "programChangeNumber": 0
        },
        {
            "outChannel": 16,
            "programChangeNumber": 3
        }
    ],
    "bankSelects": [
        {
            "outChannel": 6,
            "bankNumber": 1
        },
        {
            "outChannel": 16,
            "bankNumber": 1
        }
    ]
}
```

Note that the order of the output notes you write matters: some arpeggiators will arpeggiate the notes you input in the order you input them!

What's important to consider is that, for the zones that have some harmonies, the Zonifier will work in monophony to avoid undesired effects (please, do not press more than two notes at a time on the same zone, or you will experience some undesired behavior).