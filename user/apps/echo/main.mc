import "std";
import "range";

fn main(argc: int64, argv: uint8**) -> int64 {
    let r = struct range { start = 1, end = argc };
    for i in &r {
        print("%s ", argv[i]);
    }
    println("");
    return 0;
}
