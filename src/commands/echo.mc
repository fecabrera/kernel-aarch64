import "std";

fn command_echo(argc: int64, argv: uint8**) -> int64 {
    println("%s", argc > 1 ? argv[1] : "");
    return 0;
}
