#ifndef IECSD_H
#define IECSD_H

#include <IECFileDevice.h>
#include <SdFat.h>

#define IECSD_BUFSIZE 64

class IECSD : public IECFileDevice
{
 public: 
  IECSD(byte pinATN, byte pinCLK, byte pinDATA, byte pinRESET, byte pinChipSelect, byte pinLED);
  void begin(byte devnr);
  void task();

 protected:
  virtual void open(byte channel, const char *name);
  virtual bool write(byte channel, byte data);
  virtual byte read(byte channel, byte *buffer, byte bufferSize);
  virtual void close(byte channel);
  virtual void getStatus(char *buffer, byte bufferSize);
  virtual void execute(const char *command, byte len);
  virtual void reset();

 private:
  bool checkCard();
  byte openFile(byte channel, const char *name);
  byte openDir();
  bool readDir(byte *data);
  bool isMatch(const char *name, const char *pattern);
  void toPETSCII(byte *name);
  void fromPETSCII(byte *name);

  const char *findFile(const char *name);

  SdFat m_sd;
  SdFile m_file, m_dir;
  bool m_cardOk;

  byte m_pinLED, m_pinChipSelect, m_errorCode, m_scratched;
  byte m_dirBufferLen, m_dirBufferPtr;
  char m_dirBuffer[IECSD_BUFSIZE];
};

#endif
