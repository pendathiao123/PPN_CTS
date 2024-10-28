# PPN Project: CTS (Cryptocurrency Trading Simulator):


> Ce fichier se veut comme un fichier de suivi et **toute modification de l'orientation** du projet doit y être mentionée. **Ne sert pas de rapport**, mais peut grandement y contribuer!

## Sommaire:

[-Description et objectifs du projet](/README.md#description-et-objectifs-du-projet)

[-Attribution des rôles](/README.md#attribution-des-rôles)

[-Régles (Harmonisation) du travail](/README.md#régles-harmonisation-du-travail)

[-To do list](/README.md#to-do-list)


## Description et objectifs du projet
Description officielle en anglais:
https://academic.liparad.uvsq.fr/m1chps/ppn/projets.html#cts-cryptocurrency-trading-simulator

L'objectif durant ce premier semestre est de coder une version sequentielle d'un modèle client serveur, avec un client et un serveur.

Le client (un bot) se reveillera de temps à autres pour se connecter au serveur, envoyer des requêtes (achat, vente, recuperation d'information, ...) et se deconnecter.

Le serveur lui, tout le temps actif, gere la ou les monnaies virtuelles ainsi que le client. Il verifie que tout marche bien et qu'il n'y a pas eu d'ingèrence.

Le programme devra pouvoir fonctionner sur une machine dedié et devra faire preuve de robustesse.

Des protocoles de securité pourront (devront) être implementés, ainsi qu'une interface pour au moins lancer le programme, au plus visualiser les actions de chacun, la detection de fraudes, ...

Il faudra garder en tête que celui-ci sera confronté à une parallélisation (plusieurs clients) au deuxième semestre.

## Attribution des rôles

- Patrick:
    - Alimenter le serveur en terme de donnnées.
    - Faire le Cmake
    - Structurer le code du projet
- Penda:
    - Transactions 
    - Historique de celles-ci
- Julien: 
    - Definir les comportements des Client
    - Cretion des crypto-monaies
- Estelle:
    - Implementation des comportements des Clients

## Régles (Harmonisation) du travail

L'intégralité du code se veut en `C++`. Quelques exceptions peuvent être faites mais elles devront être duement justifiés. Ainsi le code suivra une architecture Orinté Objet, pour plus de modularité.
Le code devra être rigoureusement commenté.

Les fonctions non triviales (style `void print(){}`) doivent être testés.

La compilation se fera avec CMake (prenons notre courage à deux mains).

L'application des regles de base de gestion d'un depôt Github nous fairont aussi du bien. (Voir cours de Génie Logiciel)

On pourrait recourir à l'utilisation d'un outil de formatage pour formaliser le code que chacun pourra produire. Au cas où, moi je conseillearais de faire comme ceci:
- indentetion de 4 espaces !
- acolades sans saut à la ligne pour l'ouverture d'une fonction
- le nom de classes ainsi que le fichiers de nom de classe doivent commencer avec une majuscule (`Toto.cpp` et pas `toto.cpp`)
- même nom pour une classe et un fichier si ce fichier doit uniquement contenir la classe
- un fichier par classe (à voir ...)

## To do list

##### -> Voir directement sur Bitrix24