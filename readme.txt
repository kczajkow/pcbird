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

ladowanie modulu poleceniem (wg isr.pdf), w katalogu uio_driver/:

# sudo insmod ./pcbird_uio.ko port=0x400 irq=15
# sudo rmmod pcbird_uio

UWAGA 2 (Tomasz Kornuta):

Obecnie ustawione ustawienia karty:
Port=0x300 irq 5

Ustawienia mozna znalezc w dokumentacji karty, ktora obecnie r�wniez jest w
SVN.

