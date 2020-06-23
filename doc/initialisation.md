# Initialisation

Au moment du démarrage, la configuration est lue d'un fichier en format json nommé config.json en mémoire flash (SPIFFS). Si le fichier n'est pas présent ou que le checksum est fautif, il est initialisé à des valeurs par défaut.

La connexion WiFi est ensuite initialisée. Après 5 essais en mode STA, si le réseau ne répond pas, le système tombera en mode AP (serveur WiFi) en attente d'une connexion web pour configurer. SSID: portes, Mot de passe: portes (ou selon le mot de passe AP_PWD se trouvant dans le fichier include/secure.h). L'adresse ip sera 192.168.1.1 . Pour se connecter en mode web, l'URL sera alors http://192.168.1.1

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

Lorsque le led RUN est alumer, si le led ERROR est également alumé, cela signifie que la configuration nécessite une mise à jour des paramètres via l'interface WEB.
