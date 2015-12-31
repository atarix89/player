This project uses Waveout interface (no DirectX) to play PCM format files.

It contains:

  -Standard Win32 dialogs/controls.
  
  -Multiple threads(3).
  
  -Sound device and audio block operations.
  
  -File pointer operations and RIFF structure reading.
  
  -Comments on not-so-obvious parts of the code.
  
It lacks:

  -.mp3 or other files support. It cannot open anything that is not PCM(example of .wav file encoded with audacity is provided 
in Release folder). It will be able to open anything after adding [anything]->PCM conversion mechanisms.

  -Few error checks.
  
All functions(the whole project) are in player.cpp file. 
