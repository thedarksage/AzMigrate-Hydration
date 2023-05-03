#include <qapplication.h>
#include <iostream>
#include <sstream>
#include <string>
#include <vector>

#include "unlockvolumes.h"
#include "nounlockvolumes.h"

using namespace std;

int main( int argc, char ** argv )
{
    QApplication a( argc, argv );
    UnlockDrives w;
    NoUnlockDrives n;
    if (w.noLockedDrives()) {
      n.show();
    }
    else {
      w.show();
    }
    a.connect( &a, SIGNAL( lastWindowClosed() ), &a, SLOT( quit() ) );
    return a.exec();
}
