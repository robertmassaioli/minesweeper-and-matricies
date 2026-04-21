#ifndef __ROBERTM_LOGGING
#define __ROBERTM_LOGGING

#include <iostream>

enum class LogLevel { ERROR = 0, WARN = 1, INFO = 2, DEBUG = 3, TRACE = 4 };

class logger
{
   public:
      enum special
      {
         endl
      };

     virtual ~logger() = default;

     virtual logger& operator<<(const char* c) = 0;
     virtual logger& operator<<(std::string str) = 0;
     virtual logger& operator<<(bool b) = 0;
     virtual logger& operator<<(char c) = 0;
     virtual logger& operator<<(int i) = 0;
     virtual logger& operator<<(long l) = 0;
     virtual logger& operator<<(double d) = 0;
     virtual logger& operator<<(unsigned int i) = 0;
     virtual logger& operator<<(long unsigned int i) = 0;
     virtual logger& operator<<(special s) = 0;

     // Returns *this if level <= threshold, otherwise a silent no-op logger.
     // Use: log->at(LogLevel::DEBUG) << "message";
     virtual logger& at(LogLevel level) = 0;
};

class nop_logger : public logger
{
   public:
     logger& operator<<(const char* c)          { return *this; }
     logger& operator<<(std::string str)        { return *this; }
     logger& operator<<(bool b)                 { return *this; }
     logger& operator<<(char c)                 { return *this; }
     logger& operator<<(int i)                  { return *this; }
     logger& operator<<(long l)                 { return *this; }
     logger& operator<<(double d)               { return *this; }
     logger& operator<<(unsigned int i)         { return *this; }
     logger& operator<<(long unsigned int i)    { return *this; }
     logger& operator<<(logger::special s)      { return *this; }
     logger& at(LogLevel)                       { return *this; }
};

class ostream_logger : public logger
{
   private:
      std::ostream& s;
      LogLevel threshold_;
      nop_logger nop_;

   public:
      ostream_logger(std::ostream& stream, LogLevel threshold = LogLevel::DEBUG)
         : s(stream), threshold_(threshold)
      {}

      void setLevel(LogLevel l) { threshold_ = l; }
      LogLevel getLevel() const { return threshold_; }

      // Returns *this when level <= threshold; otherwise the silent nop_ member.
      logger& at(LogLevel level) override
      {
         return level <= threshold_
            ? static_cast<logger&>(*this)
            : static_cast<logger&>(nop_);
      }

     logger& operator<<(const char* c) override
     {
        s << c;
        return *this;
     }

     logger& operator<<(std::string str) override
     {
        s << str;
        return *this;
     }

     logger& operator<<(bool b) override
     {
        s << (b ? "true" : "false");
        return *this;
     }

     logger& operator<<(char c) override
     {
        s << c;
        return *this;
     }

     logger& operator<<(int i) override
     {
        s << i;
        return *this;
     }

     logger& operator<<(long l) override
     {
        s << l;
        return *this;
     }

     logger& operator<<(double d) override
     {
        s << d;
        return *this;
     }

     logger& operator<<(unsigned int i) override
     {
        s << i;
        return *this;
     }

     logger& operator<<(long unsigned int i) override
     {
        s << i;
        return *this;
     }

     logger& operator<<(logger::special spec) override
     {
        switch(spec)
        {
           case endl:
              s << std::endl;
              break;

           default:
              // do nothing
              break;
        }
        return *this;
     }
};

#endif
