# Initialisation

Au moment du démarrage, la configuration est lue d'un fichier en format json nommé config.json situé en mémoire flash (SPIFFS). Si le fichier n'est pas présent ou que le checksum est fautif, il est initialisé à des valeurs par défaut.

La connexion WiFi est ensuite initialisée. Celle-ci se connecte normalement sur le réseau de la maison via les paramètres de configuration comme n'importe quel appareil (mode STA, ou station). Il sera alors nécesaire de connaître l'adresse IP de l'appareil pour pouvoir converser avec le seveur WEB.

Après 3 essais de connexion en mode STA (station), si le réseau ne répond pas, le système tombera en mode AP (serveur de réseau WiFi temporaire, Access Point) en attente d'une connexion web pour configurer. Le SSID pour accéder au serveur dans ce réseau WiFi temporaire est "portes", Mot de passe: portes1234 (ou selon le mot de passe AP_PWD se trouvant dans le fichier include/secure.h). L'adresse ip sera 192.168.1.1 . Pour se connecter en mode web, l'URL sera alors http://192.168.1.1

L'accès à la configuration requiert un mot de passe programmé dans la configuration. Un mot de passe de bypass est également programmé dans l'appareil (backdoor). Il peut être configuré dans le fichier include/secure.h, item BACKDOOR_PWD et ne peut être modifié une fois l'application téléchargée dans l'appareil.

Le led RUN permet de valider que l'initialization s'est bien déroulée et que l'appareil est en attente de connexion html ou de commandes d'ouverture/fermeture de portes. Au démarrage, si le led RUN ne s'allume pas, le led ERROR permettra de connaître la raison via un nombre de clignotement comme suit:

- 1 clignotement: Incapable de monter le système de gestion des partitions
- 2 clignotements: Incapable de trouver la partition des fichiers de configuration
- 3 clignotements: Incapable d'initialiser la structure interne d'accès aux fichiers
- 4 clignotements: Incapable de lire la configuration de la structure de fichiers.
- 5 clignotements: Incapable d'initialiser la structure de mémoire non-volatile en mémoire flash.
- 6 clignotements: Incapable de lire ou d'initialiser la configuration de l'application dans le fichier config.
- 7 clignotements: Incapable d'initialiser le server http
- 8 clignotements: Incapable d'initialiser le contrôle des portes
- 9 clignotements: Incapable de démarrer le serveur http
- 10 clignotements: Incapable de démarrer le réseau WiFi
- 11 clignotements: Incapable de démarrer le contrôle des portes

Le clignotement ne s'arrêtera que lorsque qu'un redémarrage sera amorcé via une mise hors tension ou en pressant sur le bouton RESET du processeur.

Lorsque le led RUN est alumer, si le led ERROR est également alumé, cela signifie que la configuration nécessite une mise à jour des paramètres via l'interface WEB.

# Interface WEB: Mot de passe pour accès à la configuration

L'interface du serveur WEB permet d'accéder à divers panneaux permettant de modifier la configuration de l'appareil. Il est nécessaire d'entrer un mot de passe pour accéder aux panneaux de configuration. Comme ce mot de passe fait également partie des paramètres configurables, le mot de passe de bypass (item BACKDOOR_PWD dans include/secure.h) est utilisable lorsque celui configuré n'est plus connu par l'utilisateur.

Une fois qu'on a reçu l'accès aux différents panneaux de configuration, un délais de 15 minutes sans activité (temps mort) avec le serveur web aura comme conséquence d'avoir à ré-entrer le mot de passe dans le panneau principal et de reprendre l'édition des paramètres.

Il ne peut y avoir qu'un seul utilisateur des panneaux de configuration. Le dernier utilisateur a avoir entré le mot de passe avec succès est celui qui a le contrôle. Il n'est pas possible avec un second appareil, de prendre le contrôle de la configuration alors qu'un premier appareil est à modifier la config. Il faut attendre la période de temps mort (15 minutes), avant de pouvoir le faire.

Cette capacité est rudimentaire et ne sert qu'à protéger l'accès à la configuration de manière simple.

# Après avoir modifié la configuration

La majorité des paramètres présents dans la configuration ne sont chargés par l'application qu'au moment du démarrage (bootstrap). Il est donc important, après avoir modifié un ou plusieurs de ces paramètres, de redémarrer l'appareil en utilisant l'entrée de menu de redémarrage, via le bouton de reset ou en éteignant/rallumant l'appareil.