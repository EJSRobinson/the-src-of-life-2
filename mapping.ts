export type tree = {
  label: string;
  stem: number[];
  branches: tree[] | null;
};

export const roomTree: tree = {
  label: 'root',
  stem: [],
  branches: [
    {
      label: '1',
      stem: [0, 1, 3, 4, 5, 6, 7, 8],
      branches: [
        {
          label: '1.1',
          stem: [9, 10, 11, 12, 13],
          branches: [
            {
              label: '1.1.1',
              stem: [14],
              branches: [
                {
                  label: '1.1.1.1',
                  stem: [15, 16],
                  branches: null,
                },
                {
                  label: '1.1.1.2',
                  stem: [20, 19, 18, 17],
                  branches: null,
                },
              ],
            },
            {
              label: '1.1.2',
              stem: [21, 22, 23, 24, 25],
              branches: null,
            },
          ],
        },
        {
          label: '1.2',
          stem: [46, 45, 44, 43, 42, 41],
          branches: [
            {
              label: '1.2.1',
              stem: [26, 27, 28, 29, 30, 31, 32, 33],
              branches: null,
            },
            {
              label: '1.2.2',
              stem: [40, 39, 38, 37, 36, 35, 34],
              branches: null,
            },
          ],
        },
        {
          label: '1.3',
          stem: [47, 48, 49, 50, 51, 52, 53, 54],
          branches: [
            {
              label: '1.3.1',
              stem: [55, 56, 57, 58, 59],
              branches: null,
            },
            {
              label: '1.3.2',
              stem: [68],
              branches: [
                {
                  label: '1.3.2.1',
                  stem: [69, 70, 71, 72, 73],
                  branches: null,
                },
                {
                  label: '1.3.2.2',
                  stem: [67, 66, 65, 64, 63, 62, 61, 60],
                  branches: null,
                },
              ],
            },
          ],
        },
      ],
    },
    {
      label: '2',
      stem: [
        98, 99, 100, 101, 102, 103, 104, 105, 106, 107, 108, 109, 110, 111, 112, 113, 114, 115,
      ],
      branches: [
        {
          label: '2.1',
          stem: [80, 79, 78, 77, 76, 75, 74],
          branches: null,
        },
        {
          label: '2.2',
          stem: [116, 117, 118, 119, 120, 121, 122, 123],
          branches: null,
        },
      ],
    },
    {
      label: '3',
      stem: [97, 96, 95, 94, 93, 92, 91, 90, 89, 88, 87, 86, 85, 84, 83, 82, 81],
      branches: [
        {
          label: '3.1',
          stem: [116, 117, 118, 119, 120, 121, 122, 123],
          branches: null,
        },
        {
          label: '3.2',
          stem: [144, 143, 142, 141, 129],
          branches: [
            {
              label: '3.2.1',
              stem: [127, 126, 125, 124],
              branches: null,
            },
            {
              label: '3.2.2',
              stem: [132],
              branches: null,
            },
          ],
        },
        {
          label: '3.3',
          stem: [145, 146, 147],
          branches: [
            {
              label: '3.3.1',
              stem: [148, 139],
              branches: [
                {
                  label: '3.3.3.1',
                  stem: [140, 128],
                  branches: null,
                },
                {
                  label: '3.3.3.2',
                  stem: [133],
                  branches: null,
                },
              ],
            },
            {
              label: '3.3.2',
              stem: [149, 150, 151, 152, 153],
              branches: null,
            },
          ],
        },
      ],
    },
    {
      label: '4',
      stem: [193, 192, 191, 190, 189, 188, 187, 186],
      branches: [
        {
          label: '4.1',
          stem: [169, 170, 171, 172],
          branches: [
            {
              label: '4.1.1',
              stem: [173, 174],
              branches: null,
            },
            {
              label: '4.1.2',
              stem: [175, 153, 159, 156],
              branches: [
                {
                  label: '4.1.2.1',
                  stem: [157],
                  branches: null,
                },
                {
                  label: '4.1.2.2',
                  stem: [155, 154],
                  branches: null,
                },
              ],
            },
          ],
        },
        {
          label: '4.2',
          stem: [168, 167, 166, 165, 164, 163, 162, 161, 160, 176, 177, 178, 179],
          branches: [
            {
              label: '4.2.1',
              stem: [179, 180],
              branches: null,
            },
            {
              label: '4.2.2',
              stem: [183, 184, 185],
              branches: null,
            },
            {
              label: '4.2.3',
              stem: [181, 182],
              branches: null,
            },
          ],
        },
      ],
    },
  ],
};

import ws281x from 'rpi-ws281x-native';

const ledLength = 194;

const channel = ws281x(ledLength, {
  stripType: ws281x.stripType.WS2811,
  gpio: 21,
  brightness: 255,
});

type cursor = {
  element: number | null;
  subject: tree;
};

let cursors: cursor[] = [];

const wait = (ms: number) => new Promise((resolve) => setTimeout(resolve, ms));

export const wave = async (timestep: number, color: number) => {
  // console.log('Start Loop');
  while (cursors.length > 0) {
    const colorArray = channel.array;
    let deleteMarkedCursors: number[] = [];
    for (let i = 0; i < cursors.length; i++) {
      // Loop through cursors
      const cursor = cursors[i];
      if (cursor.element === null) {
        cursor.element = cursor.subject.stem[0];
      } else {
        const newIndex = cursor.subject.stem.indexOf(cursor.element) + 1;
        if (newIndex < cursor.subject.stem.length) {
          cursor.element = cursor.subject.stem[newIndex];
        } else {
          if (cursor.subject.branches === null) {
            // remove this cursor from cursors array
          } else {
            for (let j = 0; j < cursor.subject.branches.length; j++) {
              cursors.push({
                element: null,
                subject: cursor.subject.branches[j],
              });
            }
          }
          deleteMarkedCursors.push(i);
        }
      }
      colorArray[cursor.element] = color;
    }
    // render
    ws281x.render(colorArray);

    // Remove cursors that have reached the end of their stem
    const newCursors: cursor[] = [];
    for (let i = 0; i < cursors.length; i++) {
      if (!deleteMarkedCursors.includes(i)) {
        newCursors.push(cursors[i]);
      }
    }
    cursors = newCursors;
    deleteMarkedCursors = [];

    // Reset too off
    await wait(timestep);
    for (let i = 0; i < colorArray.length; i++) {
      colorArray[i] = 0x000000;
    }
  }
};

// setInterval(() => {
//   cursors.push({
//     element: null,
//     subject: roomTree,
//   });
//   // generate random color by randomizing the red, green, and blue values
//   const red = Math.floor(Math.random() * 255);
//   const green = Math.floor(Math.random() * 255);
//   const blue = Math.floor(Math.random() * 255);
//   const color = (red << 16) | (green << 8) | blue;
//   wave(1000 / 30, color);
// }, 1100);
