from numpy import sqrt 

# numero = int(input('Numero per cui cercare i divisori'))
# divisori = []
# for i in range(2,round(np.sqrt(numero))):
#     if numero %i == 0:
#         divisori.append(i)
# print(divisori)

# divisori= [divisore for divisore in range(2, round(np.sqrt(numero))) if numero % divisore == 0]

n = int(input('Numero per cui cercare i divisori'))
lunghezza_barra = int(input('lunghezza barra:'))
divisori = []

for i in range(1, round(sqrt(n))+1 ):
    if n % i == 0:
    # Trova il co-fattore
        cofattore = n // i
        divisori.append(i)
        if i != cofattore:
            divisori.append(cofattore)
    divisori.sort()       
divisori.sort()
print(divisori)

for divisore in divisori:
    if lunghezza_barra <= divisore:
        lunghezza_pezzo = divisore
        break

print(lunghezza_pezzo)