# 📋 Spécification Métier — `local-file-transfer`

**Feature** : Système de transfert de fichiers via réseau local
**Stack imposée** : C++ (C++17+) / SFML / CMake / Cross-platform Mac ↔ Windows
**Validé le** : 2026-04-18

---

## 1. Contexte

L'utilisateur possède plusieurs machines personnelles (Mac et Windows) à son
domicile, connectées au même réseau local (Wi-Fi/Ethernet). Il a régulièrement
besoin de transférer des fichiers/dossiers entre ces machines sans passer par
un service cloud tiers, une clé USB, ou une configuration réseau complexe
(SMB, FTP, etc.).

## 2. Objectif

Fournir une **application desktop** simple, rapide et cross-platform
(Mac/Windows) permettant de **découvrir automatiquement** les autres appareils
du réseau local et de **transférer fichiers/dossiers** en **peer-to-peer**
avec une expérience utilisateur fluide (type AirDrop / LocalSend).

## 3. Acteurs

- **Émetteur** : utilisateur qui envoie un ou plusieurs fichiers/dossiers
- **Destinataire(s)** : une ou plusieurs machines qui reçoivent
- **Usage** : personnel (domicile), machines de confiance

## 4. Règles Métier

### Découverte
- Détection **automatique** des autres instances de l'app sur le même LAN
- Liste rafraîchie en temps réel (apparition/disparition)
- Chaque appareil a un **nom lisible** (ex: "Mac de Serge", "PC Bureau")

### Transfert
- Support de **tous types de fichiers** (pas de filtrage MIME)
- Support des **dossiers** (arborescence complète préservée)
- **Taille max supportée : 100 Go** par fichier ou dossier total
- Possibilité d'envoyer **plusieurs fichiers en une fois** (batch)
- Possibilité d'envoyer vers **un destinataire unique** OU **plusieurs
  destinataires simultanément** (broadcast)

### Sécurité (LAN de confiance)
- Pas de chiffrement TLS des données (simple)
- **Code de vérification à 4-6 chiffres** affiché des deux côtés pour
  confirmer le pairing avant transfert
- **Accord explicite** du destinataire obligatoire avant réception
- Possibilité de **refuser** une réception

### Progression
- Affichage en temps réel : **%**, **vitesse (Mo/s)**, **temps restant estimé**

### Finalisation
- Notification côté émetteur + destinataire à la fin
- Fichiers disponibles dans un **dossier de réception configurable**
  (par défaut : `~/Downloads/LocalTransfer/`)

## 5. Cas d'Usage Principal

1. L'utilisateur ouvre l'application sur sa machine
2. L'app scanne le réseau local → liste des appareils détectés affichée
3. L'utilisateur sélectionne **un ou plusieurs destinataires**
4. Il choisit les fichiers/dossiers (sélection native OU glisser-déposer)
5. Il clique sur "Envoyer"
6. Un **code de vérification** s'affiche des deux côtés
7. Le destinataire voit une notification avec : nom(s), taille totale, code
8. Le destinataire **accepte** → le transfert démarre
9. Les deux côtés voient une **barre de progression** (%, vitesse, ETA)
10. À la fin : notification "Transfert terminé" + fichiers accessibles

## 6. Cas Alternatifs / Limites

- **Destinataire refuse** → transfert annulé, émetteur notifié
- **Déconnexion réseau pendant transfert** → notification d'erreur, possibilité
  de **reprendre** ou abandonner
- **Fichier déjà existant côté destinataire** → choix : écraser / renommer /
  ignorer
- **Espace disque insuffisant** → refus automatique côté destinataire avec
  message clair
- **Appareil émetteur ou récepteur fermé en cours de transfert** → transfert
  annulé proprement

## 7. Contraintes

### Techniques
- Stack imposée : **C++ (C++17+) + SFML + cross-platform Mac/Windows**
- Build system : **CMake** (recommandé pour cross-platform)
- Pas de dépendance cloud / internet
- Hors périmètre : support mobile (iOS/Android)

### Fonctionnelles
- Zero configuration réseau requise par l'utilisateur
- Performance : exploiter la bande passante LAN (viser > 50 Mo/s sur Gigabit
  Ethernet)
- Interface simple et claire (pas de jargon technique)

## 8. Critères d'Acceptation

- [ ] L'app détecte automatiquement les autres instances sur le LAN
- [ ] Le nom de chaque appareil est lisible et personnalisable
- [ ] On peut sélectionner 1 ou N destinataires avant envoi
- [ ] On peut envoyer 1 fichier, plusieurs fichiers, ou un dossier complet
- [ ] Le glisser-déposer fonctionne sur Mac et Windows
- [ ] Un code de vérification s'affiche des deux côtés avant transfert
- [ ] Le destinataire peut accepter ou refuser
- [ ] La progression (%, vitesse, ETA) est affichée en temps réel
- [ ] Les fichiers arrivent intacts (vérification intégrité type SHA-256
      recommandée)
- [ ] L'app fonctionne sans configuration réseau (pas besoin d'IP manuelle)
- [ ] L'app est compilable et fonctionne sur Mac ET Windows
- [ ] Gestion propre des erreurs (refus, déconnexion, conflit fichier)
