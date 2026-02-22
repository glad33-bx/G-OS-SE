# Script rapide pour convertir un PSF en tableau C
with open("unifont-17.0.03.pcf", "rb") as f:
    data = f.read()
    # On saute l'en-tête (généralement 4 octets pour PSF1, 32 pour PSF2)
    # Pour PSF1 (le plus courant en 8x16) :
    font_data = data[4:4+4096] 
    
    print("unsigned char gilles_os_font[4096] = {")
    print(", ".join(hex(b) for b in font_data))
    print("};")
