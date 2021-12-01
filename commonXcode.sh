arg1="$1" # Arg 1: set to 1 to build for the game that is using the engine

# Run only if this file or the shell.nix is newer than the generated xcconfig (basically what make/Makefiles do)
if [ -z "$arg1" ] && [ ! -z $(gfind -name shell.nix -newer Config.xcconfig) ]; then # `find -name file2 -newer file1` -- "will return null if file2 is older or the same age as file1. It will return the name (and directory) of file2 if it's newer." -- https://superuser.com/questions/188240/how-to-verify-that-file2-is-newer-than-file1-in-bash/188259
    # shell.nix is newer
    :
elif [ "$arg1" -eq "1" ] && [ ! -z $(gfind -name shell.nix -newer Defenders of Paradise/DoP_Config.xcconfig) ]; then
    # shell.nix is newer
    :
elif [ -z "$arg1" ] && [ ! -z $(gfind -name commonXcode.sh -newer Config.xcconfig) ]; then
    :
elif [ "$arg1" -eq "1" ] && [ ! -z $(gfind -name commonXcode.sh -newer Defenders of Paradise/DoP_Config.xcconfig) ]; then
    :
else
    # Nothing to do
    echo "No new changes"
    exit 0
fi

SRC_ROOT="$SRCROOT/.."
if [ -z "$IN_NIX_SHELL" ]; then
    if [ -e ~/.nix-profile/etc/profile.d/nix.sh ]; then . ~/.nix-profile/etc/profile.d/nix.sh; fi # added by Nix installer
    
    #~/.nix-profile/bin/nix-shell --run "bash \"$0\"" -p opencv pkgconfig
    cd "$SRC_ROOT"
    echo "$SRC_ROOT"
    ~/.nix-profile/bin/nix-shell --run "bash \"$0\" $@"
    exit 0
fi

if [ "$arg1" -eq "1" ]; then
    XCCONFIG="Defenders of Paradise/DoP_Config.xcconfig" # (DoP stands for Defenders of Paradise)
else
    XCCONFIG="Config.xcconfig"
fi
echo `which pkg-config`
IFLAGS="$(pkg-config --cflags-only-I glew sdl2 gl python2) -I/nix/store/mssab7csfg19054l2iddjab7q80nw48y-openal-soft-1.19.1/ -I/nix/store/mssab7csfg19054l2iddjab7q80nw48y-openal-soft-1.19.1/include -I\"$SRC_ROOT/deps/Python/Include\""

if [ "$arg1" -eq "1" ]; then
    IFLAGS="$IFLAGS -I\"$SRC_ROOT/src\""
fi

# Use a heredoc:
cat <<InputComesFromHERE > "$XCCONFIG"
HEADER_SEARCH_PATHS=$(echo "$IFLAGS" | awk 'BEGIN { ORS=" "; RS = " " } ; { sub(/^-I/, ""); print $0 }')
//OTHER_LDFLAGS = -lm -lpthread `pkg-config --libs glew sdl2 gl python2` -L"\"$SRC_ROOT/deps/Python/build2\"" -lpython2.7 -L /nix/store/mssab7csfg19054l2iddjab7q80nw48y-openal-soft-1.19.1/lib -lopenal

// TODO: ifndef NDEBUG the `-framework CoreFoundation -lobjc` in here:
OTHER_LDFLAGS = -lm -lpthread `pkg-config --libs glew sdl2 gl python2` -lpython2.7 -L /nix/store/mssab7csfg19054l2iddjab7q80nw48y-openal-soft-1.19.1/lib -lopenal -framework CoreFoundation -framework Foundation

GCC_PREPROCESSOR_DEFINITIONS = \$(inherited) EXTERNAL_DRIVER=1 GL_SILENCE_DEPRECATION=1

//OTHER_CFLAGS = $(inherited) -framework CoreFoundation
InputComesFromHERE
