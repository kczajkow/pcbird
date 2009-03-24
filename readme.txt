Projekt ISR - 24 czerwca 2008

Narzędzia do obsługi czujnika pcBIRD.

Katalogi:
-birdlclient - biblioteka kliencka + przykładowy program,
-birdsv - serwer pcBIRD,
-lsuio - narzędzie do listowania urządzeń UIO,
-uio_driver - driver UIO dla pcBIRDa (moduł do jądra),
-linux - katalog z konfiguracją jądra (UIO).

Testowane na jądrze 2.6.24.7.

Tomasz Adamczyk,
Bartosz Bielawski,
Tomasz Włostowski.

UWAGA (Piotr Trojanek):

ladowanie modulu poleceniem (wg isr.pdf):

# insmod ./pcbird_uio.ko port=0x400 irq=15
