### To build this branch do

```bash
git clone --recursive --single-branch --branch "GROTH16" "https://github.com/FreeTON-Network/FreeTON-Node.git" "FreeTON-Node-GROTH16"
cd FreeTON-Node-GROTH16
mkdir build && cd build
cmake .. -G "Ninja" -DCMAKE_BUILD_TYPE=Release -DPORTABLE=ON
ninja
```
