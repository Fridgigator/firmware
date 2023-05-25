use std::io::Result;
fn main() -> Result<()> {
    prost_build::compile_protos(
        &[
            "../../protobufs/firmware_backend.proto",
            "../../protobufs/firmware_firmware.proto",
        ],
        &["../../protobufs/"],
    )?;
    let mut config = prost_build::Config::new();
    config.btree_map(["."]);
    Ok(())
}
