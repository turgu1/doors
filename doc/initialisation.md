# Initialisation

Au moment du démarrage, la configuration est lue d'un fichier en format json nommé config.json en mémoire flash (SPIFFS). Si le fichier n'est pas présent ou que le checksum est fautif, il est initialisé à des valeurs par défaut.

La connexion WiFi est ensuite initialisée. Après 5 essais en mode STA, si le réseau ne répond pas, le système tombera en mode AP (serveur WiFi) en attente d'une connexion web pour configurer. SSID: portes, Mot de passe: portes. L'adresse ip sera 192.168.1.1 . Pour se connecter en mode web, l'URL sera alors http://192.168.1.1

L'accès à la configuration requiert un mot de passe programmé dans la configuration. Un mot de passe de bypass est programmé dans l'appareil.
