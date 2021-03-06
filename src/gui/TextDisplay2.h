// This file is part of flobby (GPL v2 or later), see the LICENSE file

#pragma once

#include <FL/Fl_Text_Display.H>
#include <string>

class LogFile;
class Fl_Text_Buffer;

class TextDisplay2: public Fl_Text_Display
{
public:
    TextDisplay2(int x, int y, int w, int h, LogFile* logFile = nullptr, char const * label = 0);
    virtual ~TextDisplay2();

    void append(std::string const & text, int interest = 0);

    enum {
        STYLE_TIME = 0,
        STYLE_LOW,
        STYLE_NORMAL,
        STYLE_HIGH,
        STYLE_MYTEXT,
        STYLE_COUNT
    };
    static Fl_Text_Display::Style_Table_Entry textStyles_[STYLE_COUNT];
    static void initTextStyles(); // call after setting font size

private:
    Fl_Text_Buffer * text_;
    Fl_Text_Buffer * style_;

    LogFile* logFile_;

    int handle(int event);

};
