#ifndef IECSD_H
#define IECSD_H

#if defined(ARDUINO_ARCH_RP2040) || defined(ESP_PLATFORM) || defined(__SAM3X8E__)
// Un-comment the line below if you have the VDrive library (https://github.com/dhansel/VDrive) 
// installed. This will allow to "CD" into Commodore disk image files (D64/G64 etc).
//#define HAVE_VDRIVE
#endif

#include <IECFileDevice.h>
#include <SdFat.h>
#ifdef HAVE_VDRIVE
#include <VDriveClass.h>
#endif

#define IECSD_BUFSIZE 64

class IECSD : public IECFileDevice
{
 public: 
  IECSD(uint8_t devnum, uint8_t pinChipSelect, uint8_t pinLED);

 protected:
  virtual void begin();
  virtual void task();

  virtual bool open(uint8_t channel, const char *name);
  virtual uint8_t read(uint8_t channel, uint8_t *buffer, uint8_t bufferSize, bool *eoi);
  virtual uint8_t write(uint8_t channel, uint8_t *buffer, uint8_t bufferSize, bool eoi);
  virtual void close(uint8_t channel);
  virtual void getStatus(char *buffer, uint8_t bufferSize);
  virtual void execute(const char *command, uint8_t len);
  virtual void reset();

#if defined(SUPPORT_EPYX) && defined(SUPPORT_EPYX_SECTOROPS) && defined(HAVE_VDRIVE)
  virtual bool epyxReadSector(uint8_t track, uint8_t sector, uint8_t *buffer);
  virtual bool epyxWriteSector(uint8_t track, uint8_t sector, uint8_t *buffer);
#endif

 private:
  bool checkCard();
  uint8_t openFile(uint8_t channel, const char *name);
  uint8_t openDir();
  bool readDir(uint8_t *data);
  bool isMatch(const char *name, const char *pattern, uint8_t extmatch);
  void toPETSCII(uint8_t *name);
  void fromPETSCII(uint8_t *name);

  const char *findFile(const char *name, char ftype);

  SdFat m_sd;
  SdFile m_file, m_dir;
  bool m_cardOk;

#ifdef HAVE_VDRIVE
  VDrive *m_drive;
#endif

  uint8_t m_pinLED, m_pinChipSelect, m_errorCode, m_scratched;
  uint8_t m_dirBufferLen, m_dirBufferPtr;
  char m_dirBuffer[IECSD_BUFSIZE];
};

#endif
