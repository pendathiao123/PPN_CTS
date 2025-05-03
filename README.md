# Projet Professionnel Numérique : Cryptocurrency Trading Simulator

> Tuteurs: Hugo Bolloré, [Salah Ibnamar](https://github.com/yaspr)
>
> Membres de l'equipe de developpement:  [Estelle Oliveira](https://github.com/estelleoliveira), [Penda Thiao](https://github.com/pendathiao123), [Patrick Nsundi Mboli](https://github.com/ElitePat), [Julien Rigal](https://github.com/Julien30127)


## Sommaire:

1. [Description et objectifs du projet](/README.md#1description-et-objectifs-du-projet)
2. [Structuration du projet](/README.md#2structuration-du-projet)
3. [Mise en place dans son environement](/README.md#3mise-en-place-dans-son-environement)
    - [Dispositions minimales](/README.mD#dispositions-minimales)
    - [Lancement du programme](/README.md#lancement-du-programme)
4. [Améliorations à venir](/README.md#4améliorations-à-venir)

## 1.Description et objectifs du projet
Description officielle en anglais:
https://academic.liparad.uvsq.fr/m1chps/ppn/projets.html#cts-cryptocurrency-trading-simulator

L'objectif durant ce premier semestre est de coder une version sequentielle d'un modèle client serveur, avec un client et un serveur qui communiquent de manière sécurisé. Le client se reveillera de temps à autres pour se connecter au serveur, envoyer des requêtes (achat, vente, recuperation d'information, ...) et se deconnecter. Le serveur lui, tout le temps actif, gere la ou les monnaies virtuelles ainsi que les requêtes des clients. Il verifie que tout marche bien et qu'il n'y a pas d'ingèrences.

Durant le deuxiéme semestre l'objectif est de permettre au serveur de realiser le même travail avec plusieurs clients en parallèle. Se posent alors des problèmes d'accées concurrent à des ressources limites qu'il faut résoudre.

A terme on voudrait avoir un programme capable de simuler, dans un moindre mesure, le marché des crypto-monaies avec, ici, un système centralisé (le serveur principal contôle toutes les opérations).


## 2.Structuration du projet:

Le depôt associé au projet s'articule de la maniére suivante:
- le dossier [`/documents`](/documents) contient differents fichiers dont nous nous sommes servis dans le projet ainsi que la documentation officielle: [Rapport_PPN-CTS.pdf](/documents/Rapport_PPN-CCTS.pdf).
- le dossier [`/src`](/src) contient les fichiers sources du projet ainsi que differents jeux de donnés.
- le dossier [`/FineTrading`](/FineTrading/) contient des fichiers pour faire des benchmakrs specifiquement sur le Trading.

## 3.Mise en place dans son environement:

### Dispositions minimales

Actuellement, les OS pris en charge sont Linux et WSL(Windows).

Dispositions minimales pour faire tourner le projet sur votre machine:
- Version minimale de CMake: 3.25
- Version de C++ requise: C++17
- OpenSSL 3.0.13
- Curl

> NB: l'installation des packages `libdev-(open)ssl` `libcurl4-openssl-dev` et `nlohmann-json3-dev` sera nécessaire pour compiler et executer le programme.


### Lancement du programme

Après avoir telechargé le programme ou l'avoir cloné sur sa machine à l'aide de `git clone https://github.com/pendathiao123/PPN_CTS.git`, pour le lancer en local il faut:
- se placer dans le repertoire source du projet
- Créer un répertoire `build` pour les executables et se déplacer dedans:
```bash
mkdir build
cd build
```
- Tapper la commande suivante pour compiler le programme:
```bash
cmake ..
```
- Ouvrir un autre Terminal dans le même dossier.
- Dans le premier terminal on lance le Serveur comme ceci:
```bash
make Serv &
```
- Dans le second terminal on compile le Client
```bash
make Cli
```
- On revient dans le repertoire du projet `cd ../` et execute:
```bash
./run_cli.sh
```

Alors on peut commencer à communiquer avec le programme via le terminal.

> Effacer le répertoire si nécessaire avec: 
> ```bash
> make my_clean
> ```


## 4.Améliorations à venir


Une parallélisation avec plusieurs serveurs (Serveur-MultiThreadé) reste envisagé. En plus d'une possible implementation d'une blockchain (non-trivial), pour valider toutes les transactions. Aussi l'implementation d'une interface pour lancer le programme (differents processus) est aussi envisagé.