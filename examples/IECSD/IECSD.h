#ifndef IECSD_H
#define IECSD_H

#if defined(ARDUINO_ARCH_RP2040) || defined(ESP_PLATFORM) || defined(__SAM3X8E__)
// Un-comment the line below if you have the VDrive library (https://github.com/dhansel/VDrive) 
// installed. This will allow to "CD" into Commodore disk image files (D64/G64 etc).
//#define HAVE_VDRIVE
//#define PIN_BUTTON_IMAGE_PREV 26
//#define PIN_BUTTON_IMAGE_NEXT 27
#endif

#include <IECFileDevice.h>
#include <SdFat.h>
#ifdef HAVE_VDRIVE
#include <VDriveClass.h>
#endif

#define IECSD_BUFSIZE   96
#define IECSD_MAX_PATH 128

class IECSD : public IECFileDevice
{
 public: 
  IECSD(uint8_t devnum, uint8_t pinChipSelect, uint8_t pinLED);

 protected:
  virtual void begin();
  virtual void task();

  virtual bool open(uint8_t channel, const char *name, uint8_t nameLen);
  virtual uint8_t read(uint8_t channel, uint8_t *buffer, uint8_t bufferSize, bool *eoi);
  virtual uint8_t write(uint8_t channel, uint8_t *buffer, uint8_t bufferSize, bool eoi);
  virtual void close(uint8_t channel);
  virtual void getStatus(char *buffer, uint8_t bufferSize);
  virtual uint8_t getStatusData(char *buffer, uint8_t bufferSize, bool *eoi);
  virtual void executeData(const uint8_t *data, uint8_t len);
  virtual void execute(const char *command);
  virtual void reset();

#if defined(IEC_FP_EPYX) && defined(IEC_FP_EPYX_SECTOROPS) && defined(HAVE_VDRIVE)
  virtual bool epyxReadSector(uint8_t track, uint8_t sector, uint8_t *buffer);
  virtual bool epyxWriteSector(uint8_t track, uint8_t sector, uint8_t *buffer);
#endif

 private:
  bool checkCard();
  uint8_t openFile(uint8_t channel, const char *name);
  uint8_t openDir(const char *pattern);
  bool readDir(uint8_t *data);
  bool isMatch(const char *name, const char *pattern, uint8_t extmatch);
  void toPETSCII(uint8_t *name);
  void fromPETSCII(uint8_t *name);

  uint8_t chdir(const char *c);
  const char *findFile(const char *name, uint8_t ftype);

  SdFat m_sd;
  SdFile m_file, m_dir;
  char m_cwd[IECSD_MAX_PATH+1], *m_dirPattern;
  bool m_cardOk;

#ifdef HAVE_VDRIVE
  VDrive *m_drive;
#endif

  uint8_t m_pinLED, m_pinChipSelect, m_errorCode, m_scratched;
  uint8_t m_bufferLen, m_bufferPtr, m_dirFormat;
  bool m_suppressMemExeError, m_suppressReset;
  char m_buffer[IECSD_BUFSIZE];
};

#endif
