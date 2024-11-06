#ifndef IECSD_H
#define IECSD_H

#include <IECFileDevice.h>
#include <SdFat.h>

#define IECSD_BUFSIZE 64

class IECSD : public IECFileDevice
{
 public: 
  IECSD(byte devnum, byte pinChipSelect, byte pinLED);

 protected:
  virtual void begin();
  virtual void task();

  virtual void open(byte channel, const char *name);
  virtual byte read(byte channel, byte *buffer, byte bufferSize);
  virtual byte write(byte channel, byte *buffer, byte bufferSize);
  virtual void close(byte channel);
  virtual void getStatus(char *buffer, byte bufferSize);
  virtual void execute(const char *command, byte len);
  virtual void reset();

#if defined(SUPPORT_EPYX) && defined(SUPPORT_EPYX_SECTOROPS)
  virtual bool epyxReadSector(byte track, byte sector, byte *buffer);
  virtual bool epyxWriteSector(byte track, byte sector, byte *buffer);
#endif

 private:
  bool checkCard();
  byte openFile(byte channel, const char *name);
  byte openDir();
  bool readDir(byte *data);
  bool isMatch(const char *name, const char *pattern, byte extmatch);
  void toPETSCII(byte *name);
  void fromPETSCII(byte *name);

  const char *findFile(const char *name, char ftype);

  SdFat m_sd;
  SdFile m_file, m_dir;
  bool m_cardOk;

  byte m_pinLED, m_pinChipSelect, m_errorCode, m_scratched;
  byte m_dirBufferLen, m_dirBufferPtr;
  char m_dirBuffer[IECSD_BUFSIZE];
};

#endif
