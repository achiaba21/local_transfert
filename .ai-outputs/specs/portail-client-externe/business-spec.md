# Spécification Métier — Phase 2 : Portail Client Externe

> Validée par l'utilisateur le 2026-05-12.
> Phase suivante : Architecture.

## 1. Contexte

LocalTransfer permet aujourd'hui aux utilisateurs présents sur le LAN
d'échanger des fichiers, et offre déjà un dashboard web protégé par PIN pour
le propriétaire de la machine (le « host »). Mais une situation reste mal
couverte : un **client externe** (prestataire, fournisseur, partenaire) doit
déposer des fichiers vers le host **sans** être un utilisateur de l'app,
sans installer quoi que ce soit, sans accéder au dashboard complet, et
idéalement sans connaître l'existence de l'outil.

C'est le cas d'usage couvert par les offres Business : un comptable qui
reçoit des justificatifs clients, un avocat qui reçoit des pièces de
dossier, un freelance qui reçoit des assets clients.

## 2. Objectif

Permettre au host de **créer, partager et superviser des liens de dépôt**
que des clients externes peuvent ouvrir dans n'importe quel navigateur du
LAN pour envoyer des fichiers, en suivant un parcours simple et rassurant
(consentement explicite, reçu téléchargeable, messages non techniques).

## 3. Acteurs

| Acteur | Rôle |
|---|---|
| **Host** | Propriétaire du desktop LocalTransfer. Crée les liens de dépôt, fixe leurs limites, reçoit les fichiers, est notifié en temps réel, consulte l'historique. |
| **Déposant** | Personne externe qui ouvre le lien dans son navigateur. Saisit son nom, accepte le consentement, dépose ses fichiers, récupère un reçu. N'a aucun compte, aucune trace persistante côté navigateur. |

## 4. Différence avec le mode web existant (positionnement validé)

| | **Web actuel (dashboard PIN)** | **Portail Phase 2 (dépôt)** |
|---|---|---|
| Authentification | PIN à connaître | Le lien suffit |
| Préparé par le host | Non (PIN dynamique) | Oui (lien avec règles) |
| Le visiteur voit le dashboard | Oui (tout) | Non (rien) |
| Sens | Envoyer + recevoir | Déposer uniquement |
| Cas type | Collègue à côté de moi | Client externe à distance |
| Identité du visiteur | Anonyme | Nom + consentement + reçu |
| Traçable comme « dépôt client » | Non | Oui, audit dédié |
| Limites par session | Non | Oui (taille, nb fichiers, durée) |

Le portail est un **mode séparé**, pas une variante du dashboard. Aucune
fuite d'info du dashboard vers la page de dépôt.

## 5. Règles Métier

### 5.1 Création d'un lien de dépôt (host uniquement)

- Le host crée un lien de dépôt depuis son app desktop (et seulement depuis
  là).
- À la création, il fixe :
  - un **libellé** (ex. « Dossier Dupont — pièces 2026 »),
  - une **durée d'expiration** : 1 h, 24 h, 7 jours (défaut), 30 jours, ou
    « sans expiration »,
  - une **taille totale maximale par dépôt** (défaut suggéré : 2 Go,
    modifiable),
  - un **nombre maximum de fichiers par dépôt** (défaut suggéré : 50,
    modifiable),
  - un **texte de consentement** affiché au déposant (un texte par défaut
    neutre est proposé, modifiable).
- Le host peut **révoquer manuellement** un lien à tout moment, même avant
  son expiration.
- Plusieurs liens peuvent coexister en parallèle.

### 5.2 Cycle de vie du lien

- Un lien est **réutilisable** : le host le partage à un ou plusieurs
  clients ; chaque visite est un **dépôt indépendant**.
- Un lien expiré n'accepte plus aucun dépôt et affiche un message clair au
  déposant : « Ce lien n'est plus actif. Contactez votre interlocuteur. »
- Un lien révoqué se comporte comme un lien expiré (pas de distinction
  visible côté déposant).

### 5.3 Parcours déposant

1. Le déposant ouvre le lien (URL ou scan QR).
2. Il voit une page **simple, sans le dashboard host**, qui affiche : le
   libellé du lien, le nom du host (= organisation si configurée dans le
   plan Business, sinon nom de l'appareil), les limites du dépôt en clair
   (taille max, nb max de fichiers), le texte de consentement.
3. Il saisit son **nom** (champ obligatoire, libre, non vérifié).
4. Il **coche** explicitement le consentement (obligatoire pour passer à
   l'étape suivante).
5. Il ajoute ses fichiers (glisser-déposer ou bouton parcourir).
6. Si une limite est dépassée, l'envoi est refusé **avant** transfert avec
   un message non technique : « Le dépôt dépasse la taille autorisée
   (X sur Y Mo). » ou « Trop de fichiers (X sur Y). »
7. À l'envoi, une barre de progression simple s'affiche (pas de tabs, pas
   de jargon).
8. À la fin, une page de confirmation s'affiche avec : un **identifiant de
   dépôt unique**, l'horodatage, la liste des fichiers envoyés, et un
   bouton **« Télécharger le reçu »** (fichier PDF ou JSON signé,
   téléchargeable localement par le déposant).
9. Aucun cookie, aucune session persistante : si le déposant revient sur
   le lien, c'est un nouveau dépôt vierge. Il ne voit pas ses dépôts
   précédents.

### 5.4 Côté host

- Une **notification temps réel** (popup desktop + son léger) prévient le
  host d'un nouveau dépôt entrant pendant que l'app tourne.
- Si l'app était fermée, la notification apparaît au prochain lancement.
- Les fichiers déposés sont rangés dans un **sous-dossier dédié par
  dépôt**, sous le dossier de téléchargement existant, nommé selon la
  convention : `{libellé-lien}/{date-iso}__{nom-déposant}__{id-dépôt-court}/`.
- L'historique global du host gagne un **filtre « Dépôts externes »** et
  un sous-filtre par lien, permettant de retrouver tous les dépôts d'un
  client donné.
- Chaque entrée d'historique de dépôt expose : libellé du lien, nom du
  déposant, nb de fichiers, taille totale, statut, horodatage, lien vers
  le reçu.

### 5.5 Intégrations existantes (Phase 1)

- **Quota Business** : un dépôt consomme le quota mensuel du host, comme
  un upload web standard. Si le quota est dépassé, le dépôt est refusé en
  amont avec le message non technique : « Ce dépôt ne peut pas être
  accepté pour le moment. Contactez votre interlocuteur. » (Pas de fuite
  du mot « quota » côté déposant.)
- **Audit** : chaque dépôt est tracé dans l'historique exportable
  (CSV/JSON), enrichi des champs `depositLinkId`, `depositLinkLabel`,
  `depositorName`, `consentAccepted`, `receiptId`.
- **Personal Free** : la fonctionnalité « créer un lien de dépôt » est
  réservée aux plans payants (Business / Business+ / Enterprise). Sur
  Personal Free, l'option est visible mais grisée avec un message
  d'upsell discret.

## 6. Cas d'Usage Principal

1. Le host (un comptable) crée un lien « Pièces Dupont 2026 », expiration
   30 jours, max 2 Go, max 50 fichiers.
2. L'app desktop affiche l'URL et un QR code.
3. Le comptable envoie l'URL à son client par email ou messagerie.
4. Le client Dupont ouvre l'URL, saisit son nom, accepte le consentement,
   glisse 12 PDF.
5. L'envoi se fait, Dupont reçoit sa page de confirmation, télécharge son
   reçu PDF.
6. Côté desktop, une notification apparaît : « Nouveau dépôt — Pièces
   Dupont 2026 (12 fichiers, 84 Mo). »
7. Le comptable retrouve les fichiers dans
   `Downloads/Pièces Dupont 2026/2026-05-12__Dupont__a3f9/`.
8. Une semaine plus tard, Dupont reçoit le rappel de son comptable,
   retourne sur le même lien et dépose 3 fichiers complémentaires — c'est
   un nouveau dépôt, dans un nouveau sous-dossier.

## 7. Cas Alternatifs / Limites

| Cas | Comportement attendu |
|---|---|
| Lien expiré | Page « Ce lien n'est plus actif. » — pas de form |
| Lien révoqué | Idem expiré (pas de distinction visible côté déposant) |
| Dépassement taille / nb fichiers | Refus avant transfert avec message non technique |
| Quota host épuisé | Refus avec message neutre côté déposant + alerte côté host |
| Réseau coupé en cours de dépôt | Page d'erreur courte + invitation à recommencer (pas de reprise auto en Phase 2) |
| Plusieurs déposants en parallèle | Supporté — chaque dépôt est isolé dans son propre sous-dossier |
| Plan Personal Free | Fonction grisée + message d'upsell — pas d'accès |
| Consentement non coché | Bouton d'envoi désactivé |
| Nom vide | Bouton d'envoi désactivé |

## 8. Contraintes

- **Étanchéité du portail** : le déposant ne doit jamais voir le dashboard
  host, ni l'historique, ni les peers, ni le mode P2P, ni le PIN. Aucune
  fuite d'information.
- **Messages non techniques** : aucun jargon (« quota », « TCP »,
  « session », « token ») n'apparaît côté déposant. Côté host, le
  vocabulaire reste cohérent avec celui du reste de l'app.
- **Sécurité de base** : le lien contient un identifiant suffisamment long
  et imprévisible pour ne pas être deviné. Le host peut révoquer un lien
  si compromis.
- **Aucune dépendance externe nouvelle** : pas d'email, pas de SMS, pas
  de cloud, pas de service tiers (pas de SMTP requis grâce au choix
  « nom seul »).
- **Compatible LAN seul** : le portail reste accessible uniquement sur le
  réseau local, comme le reste de l'app (pas de tunnel internet en
  Phase 2).
- **Reçu vérifiable** : le reçu téléchargé par le déposant doit pouvoir
  être présenté plus tard au host pour confirmer l'authenticité d'un
  dépôt (id + signature).

## 9. Critères d'Acceptation

- [ ] Le host peut créer un lien de dépôt avec libellé, expiration, taille
  max, nb fichiers max, texte de consentement, depuis l'app desktop.
- [ ] L'app desktop affiche l'URL et le QR code du lien, et permet de le
  copier.
- [ ] Le host voit la liste de ses liens actifs et peut en révoquer un.
- [ ] Un déposant ouvrant l'URL voit une page séparée du dashboard, sans
  accès aux autres fonctionnalités.
- [ ] Le déposant ne peut envoyer que s'il a saisi un nom **et** coché le
  consentement.
- [ ] Un dépôt dépassant les limites du lien est refusé avec un message
  non technique.
- [ ] Un dépôt sur un lien expiré ou révoqué affiche un message clair sans
  formulaire.
- [ ] Les fichiers déposés arrivent dans un sous-dossier dédié au dépôt
  sur la machine host.
- [ ] Une notification temps réel informe le host d'un nouveau dépôt.
- [ ] Le déposant reçoit un identifiant de dépôt + page de confirmation +
  reçu téléchargeable.
- [ ] L'historique host est filtrable par « dépôts externes » et par lien.
- [ ] L'export d'audit (CSV/JSON) inclut les champs spécifiques au dépôt.
- [ ] Un dépôt est compté dans le quota Business mensuel du host.
- [ ] Sur Personal Free, la création de liens est désactivée avec un
  message d'upsell.
