# Projet Professionnel Numérique : Cryptocurrency Trading Simulator


> Membres de l'equipe de developpement:  [Estelle Oliveira](https://github.com/estelleoliveira), [Penda Thiao](https://github.com/pendathiao123), [Patrick Nsundi Mboli](https://github.com/ElitePat), [Julien Rigal](https://github.com/Julien30127)

## Sommaire:

[1.Description et objectifs du projet](/README.md#1description-et-objectifs-du-projet)

[2.Structuration du projet](/README.md#2structuration-du-projet)

[3.Mise en place du projet](/README.md#3mise-en-place-du-projet)

[4.Améliorations à venir](/README.md#4améliorations-à-venir)


## 1.Description et objectifs du projet
Description officielle en anglais:
https://academic.liparad.uvsq.fr/m1chps/ppn/projets.html#cts-cryptocurrency-trading-simulator

L'objectif durant ce premier semestre est de coder une version sequentielle d'un modèle client serveur, avec un client et un serveur qui communiquent de manière sécurisé.

Le client (un bot) se reveillera de temps à autres pour se connecter au serveur, envoyer des requêtes (achat, vente, recuperation d'information, ...) et se deconnecter.

Le serveur lui, tout le temps actif, gere la ou les monnaies virtuelles ainsi que les requêtes des clients. Il verifie que tout marche bien et qu'il n'y a pas eu d'ingèrences.


## 2.Structuration du projet:

Le depôt associé au projet s'articule de la maniére suivante:
- dans le repertoire `/documents`:
    - le reprtoire `/annexes` contient differents graphiques, tableaux et autres fichiers dont nous nous sommes servis dans le projet
    - le reprtoire `/PPN_CTS_docs` contient la documentation officielle
- dans le repertoire `/src` (fichiers source):
    - le reprtoire `/code` contient les fichiers contenant le code des programmes principaux
    - le reprtoire `/data` contient différents jeux de données
    - le reprtoire `/headers` contient les entêtes des fichiers `.cpp` presents dans `/src/code`

## 3.Mise en place de l'environement:

Le projet, actuellement, est seulement conçu pour tourner sous les distributions Linux (WSL est aussi supporté).

Dispositions minimales pour faire tourner le projet sur votre machine:
- Version minimale de CMake: 3.10
- C++17 au minimum
- OpenSSL 3.0.13

Pour lancer le programme en local:
- telecharger le projet (...)
- se placer dans le repertoire source du projet
- Tapper la commande suivante:
```bash
cmake .
```
- Pour executer le Serveur puis le Client, faire comme suit:
```bash
make Serv &
make Cli
```



## 4.Améliorations à venir

Principalement pour le deuxiéme semestre:


Le programme sera confronté à une parallélisation avec plusieurs clients, voir plusieurs serveurs (Serveur-MultiThreadé). En plus d'une possible implementation d'une blockchain (non-trivial), pour valider toutes les transactions.

L'implementation d'une interface pour lancer le programme (differents processus) est aussi envisagé.