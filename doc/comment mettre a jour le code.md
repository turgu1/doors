# Note explicative - Comment mettre à jour le ESP32

Cette note explique comment utiliser Visual Studio Code afin de mettre à jour le programme et l'espace disque du processeur ESP32.

Les étapes sont les suivantes:

1. Connecter l'appareil à un port USB
2. Mettre à jour le code source dans VS Code
3. Détruire les fenêtres de monitoring
4. Mettre à niveau la mémoire flash (simulateur de disque SPIFFS)
5. Compiler l'application
6. Charger l'application dans la mémoire flash

L'étape 1 étant relativement simple, les étapes subséquentes sont décrites dans les paragraphes qui suivent.

## Mettre à jour le code source

Le code source de l'application est maintenu sur la plateforme Github sur Internet. Visual Studio Code intègre les fonctions permettant d'automatiser la récupération des mises à jour du code source de manière automatique. Pour se faire, il est nécessaire de presser le bouton de rafraichissement. Ce dernier est situé immédiatement à droite du mot "master" en bas de page. Une fois complété, les deux indicateurs de version situés à droite du bouton de rafraichissement devraient être à 0.

## Détruire les fenêtres de monitoring

La mise à niveau de la mémoire flash requière qu'aucune fenêtre de monitoring des activités du ESP32 ne soit en fonction. Les tâches de monitoring apparaissent dans la fenêtre de Visual Studio Code en bas de page. Dans cet espace, lorsque l'entrée de menu TERMINAL de cet espace (et non pas le menu principal de VS Code) est sélectionnée, apparaît du côté droit un symbole de poubelle. On doit cliquer sur cette poubelle pour détruire toutes les tâches de monitoring qui seraient en fonction.

## Mettre à niveau la mémoire flash

Le sous-répertoire "data" de l'environnement de développement contient les fichiers qui doivent être installés dans la mémoire flash (simulateur de disque SPIFFS). Pour se faire, il faut utiliser la tâche nommée "PlatformIO: Upload File System Image". Cette dernière initialise les partitions de la mémoire flash (si elle n'a pas déjà été initialisée) pour ensuite transférer tout le répertoire data vers cette mémoire. Attention à ne pas lancer d'autres tâches durant l'exécution, pour ne par interférer avec le processus.

La tâche "Upload File System Image" est accessible dans le menu des tâches. Ce menu est visualisé grâce au pictogramme localisé dans le bas de la fenêtre de VS Code, entre les pictogrammes de bécher et de prise de courant.

Une fois la mise à niveau complétée, le processeur sera automatiquement redémarré.

## Compiler l'application

La compilation doit être lancée via le pictogramme "crochet" situé immédiatement à droite de la maison au bas de la fenêtre de Visual Studio Code. Si tout c'est bien déroulé, le mot "SUCCESS" devrait apparaître dans l'espace de monitoring.

# Charger l'application dans la mémoire flash

Le chargement de l'application peut être lancé grâce au pictogramme "flèche droite" adjacent au pictogramme "crochet" au bas de la fenêtre de Visual Studion Code. Si tout c'est bien déroulé, le mot "SUCCESS" devrait apparaître dans l'espace de monitoring.

Une fois l'application chargée en mémoire flash, le processeur sera automatiquement redémarré.
