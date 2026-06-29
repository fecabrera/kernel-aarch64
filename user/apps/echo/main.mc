import "std";
import "range";

fn main(argc: int64, argv: char**) -> int64 {
    let r = struct range { start = 1, end = argc };
    for i in &r {
        print("%s ", argv[i]);
    }
    println("");
    return 0;
}
