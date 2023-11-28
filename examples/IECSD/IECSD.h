#ifndef IECSD_H
#define IECSD_H

#include <IECDevice.h>
#include <SPI.h>
#include <SD.h>

#define IECSD_BUFSIZE 64

class IECSD : public IECDevice
{
 public: 
  IECSD(byte pinATN, byte pinCLK, byte pinDATA, byte pinRESET, byte pinChipSelect, byte pinLED);
  void begin(byte devnr);
  void task();

 protected:
  virtual void open(byte secondary);
  virtual void close(byte secondary);
  virtual void listen(byte secondary);
  virtual void unlisten(byte secondary);
  virtual void reset();

  virtual int8_t canWrite(byte channel);
  virtual void   write(byte channel, byte data);
  virtual int8_t canRead(byte channel);
  virtual byte   read(byte channel);

 private:
  void sdTask();
  void setStatus();

  enum 
    {
      CMD_NONE = 0,
      CMD_READDIR,
      CMD_READFILE,
      CMD_WRITEFILE,
      CMD_CLOSEFILE,
      CMD_COMMAND
    };

  File m_file, m_dir;
  bool m_fileWrite;

  bool m_opening, m_writing;
  byte m_curCmd, m_pinLED, m_pinChipSelect, m_errorCode, m_scratched;
  byte m_dataBufferLen, m_dataBufferPtr, m_statusBufferLen, m_statusBufferPtr;
  char m_dataBuffer[IECSD_BUFSIZE], m_statusBuffer[IECSD_BUFSIZE];
};

#endif
