#ifndef __ROBERTM_LOGGING
#define __ROBERTM_LOGGING

#include <iostream>

class logger
{
   public:
      enum special
      {
         endl
      };

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
};

class ostream_logger : public logger
{
   private:
      std::ostream& s;

   public:
      ostream_logger(std::ostream& stream) 
         : s(stream)
      {}

     logger& operator<<(const char* c)    
     { 
        s << c;
        return *this; 
     } 

     logger& operator<<(std::string str)  
     { 
        s << str;
        return *this; 
     }

     logger& operator<<(bool b)           
     {
        s << (b ? "true" : "false");
        return *this; 
     } 

     logger& operator<<(char c)                 
     { 
        s << c;
        return *this; 
     }

     logger& operator<<(int i)            
     { 
        s << i;
        return *this; 
     } 

     logger& operator<<(long l)                 
     { 
        s << l;
        return *this; 
     }

     logger& operator<<(double d)               
     { 
        s << d;
        return *this; 
     } 

     logger& operator<<(unsigned int i)   
     { 
        s << i;
        return *this; 
     }

     logger& operator<<(long unsigned int i)    
     { 
        s << i;
        return *this; 
     }

     logger& operator<<(logger::special spec)
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
