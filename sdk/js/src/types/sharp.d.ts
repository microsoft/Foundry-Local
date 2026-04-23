declare module 'sharp' {
    interface SharpInstance {
        metadata(): Promise<{ width?: number; height?: number }>;
        resize(width: number, height: number): SharpInstance;
        png(): SharpInstance;
        toBuffer(): Promise<Buffer>;
    }
    function sharp(input: Buffer | string): SharpInstance;
    export default sharp;
}
