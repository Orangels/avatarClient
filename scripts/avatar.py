import audioop
import pickle
import time

import cv2
import numpy as np
import win32file
import win32pipe
# import torch
import yaml

from multiprocessing import Lock, Manager, Pipe, Process, Queue
from easydict import EasyDict

AUDIO_PIPE_NAME = r"\\.\pipe\avatar_audio"
VIDEO_PIPE_NAME = r"\\.\pipe\avatar_video"

def initialize_lip_model():
    return None

def iinitialize_we_model():
    return None

def read_vid_pkl(args):
    with open(args.face_id_file, 'rb') as fr:
        dict_data = pickle.load(fr)
        face_det_results = dict_data['face_det']

    full_frames = []
    video_stream = cv2.VideoCapture(args.face_vid_path)
    while 1:
        still_reading, frame = video_stream.read()
        if not still_reading:
            video_stream.release()
            break
        if args.resize_factor > 1:
            frame = cv2.resize(
                frame, (frame.shape[1] // args.resize_factor, frame.shape[0] // args.resize_factor))

        if args.rotate:
            frame = cv2.rotate(frame, cv2.cv2.ROTATE_90_CLOCKWISE)

        y1, y2, x1, x2 = args.crop
        if x2 == -1:
            x2 = frame.shape[1]
        if y2 == -1: y2 = frame.shape[0]

        frame = frame[y1:y2, x1:x2]

        full_frames.append(frame)
    return full_frames, face_det_results

def mel_2_melseq(args, mel, fps=25):
    mel_chunks = []
    mel_idx_multiplier = 25. / fps
    i = 0
    while 1:
        start_idx = int(i * mel_idx_multiplier)
        if start_idx + args.mel_step_size > mel.shape[0]:
            break
        mel_chunks.append(mel[start_idx: start_idx + args.mel_step_size])
        i += 1
    return mel_chunks
    
def read_audio(pipe_s, name=''):
    print(f'开始运行读取音频流的子进程 {name}...')
    audio_pipe = win32file.INVALID_HANDLE_VALUE
    while True:
        try:
            audio_pipe = win32file.CreateFile(AUDIO_PIPE_NAME,
                                  win32file.GENERIC_READ,
                                  0, None,
                                  win32file.OPEN_EXISTING, 0, None)
            if audio_pipe != win32file.INVALID_HANDLE_VALUE:
                break
        except Exception as e:
            pass
    print("audio pipe connected.")
    while True:
        try:
            _, data = win32file.ReadFile(audio_pipe, 1280, None)
            # print(f"received audio data {len(data)} bytes.")
            if pipe_s:
                pipe_s.send(data)
        except Exception as e:
            print(e)
            break
    win32file.CloseHandle(audio_pipe)
    
def make_audio_batch(args, pipe_r, pipe_s, d, l, name=''):
    print(f'开始运行音频流转batch的子进程 {name}...')
    list_stream = []
    list_stream_added = []
    add_flag = False
    silence_num = 0
    while True:
        data_r = pipe_r.recv()
        rms_val = audioop.rms(data_r, 2)
        if rms_val > args.sound_threshold:
            with l:
                d['flag'] = True
            silence_num = 0
        else:
            silence_num += 1

        if silence_num >= args.silence_num_threshold:
            with l:
                d['flag'] = False

        data_np_r = np.frombuffer(data_r, dtype=np.int16)

        if add_flag:
            list_stream_added.append(data_np_r)
            if len(list_stream_added) >= args.stream_batch_size + args.stream_batch_added_size:
                list_stream_added_np = np.array(list_stream_added).flatten()
                pipe_s.send(list_stream_added_np)
                add_flag = False
                list_stream_added.clear()

        list_stream.append(data_np_r)
        if len(list_stream) >= args.stream_batch_size:
            list_stream_added.extend(list_stream)
            list_stream.clear()
            add_flag = True

def mel_2_lip_Q(args, pipe_r, d, name=''):
    print(f'开始运行口型同步的子进程 {name}...')

    video_pipe = win32file.INVALID_HANDLE_VALUE
    while True:
        try:
            video_pipe = win32file.CreateFile(VIDEO_PIPE_NAME,
                                  win32file.GENERIC_WRITE,
                                  0, None,
                                  win32file.OPEN_EXISTING, 0, None)
            if video_pipe != win32file.INVALID_HANDLE_VALUE:
                break
        except Exception as e:
            pass
    print("video pipe connected.")

    # device = 'cuda' if torch.cuda.is_available() else 'cpu'

    full_frames_all, face_det_results_all = read_vid_pkl(args)
    lip_model = initialize_lip_model()
    we_model = iinitialize_we_model()
    frames_start = 0
    full_frames_all_num = len(full_frames_all)
    pcm = b'\0' * (1280)

    keep_running = True
    while keep_running:
        data_np_r = pipe_r.recv()
        # 先注掉
        # encoder_out = we_model.infer_stream(data_np_r)
        # mel_chunks = mel_2_melseq(encoder_out[0], self.args.fps)
        # mel_chunks_num = len(mel_chunks)
        mel_chunks_num = args.stream_batch_size
        if frames_start + mel_chunks_num > full_frames_all_num:
            frames_end = frames_start + mel_chunks_num - full_frames_all_num
            full_frames = full_frames_all[frames_start:] + full_frames_all[:frames_end]
            face_det_results = face_det_results_all[frames_start:] + face_det_results_all[:frames_end]
        else:
            frames_end = frames_start + mel_chunks_num
            full_frames = full_frames_all[frames_start:frames_end]
            face_det_results = face_det_results_all[frames_start:frames_end]
        frames_start = frames_end

        if d['flag']:
            for img in full_frames:
                img_c = img.copy()
                cv2.putText(img_c, "Liped", (50, 50), cv2.FONT_HERSHEY_SIMPLEX, 2, (0, 0, 255), 3)
                try:
                    result = win32file.WriteFile(video_pipe, img_c)
                    result = win32file.WriteFile(video_pipe, pcm)
                    print(result)
                except Exception as e:
                    print(e)
                    keep_running = False
                    break
            # 先注释掉
            # gen = datagen(full_frames.copy(), mel_chunks, face_det_results)
            # for i, (img_batch, mel_batch, frames, coords) in enumerate(gen):
            #     img_batch = torch.FloatTensor(np.transpose(img_batch, (0, 3, 1, 2))).to(device)
            #     mel_batch = torch.FloatTensor(np.transpose(mel_batch, (0, 3, 1, 2))).to(device)
            #     with torch.no_grad():
            #         pred = lip_model(mel_batch, img_batch)
            #
            #     pred = pred[1].cpu().numpy().transpose(0, 2, 3, 1) * 255.
            #
            #     for p, f, c in zip(pred, frames, coords):
            #         y1, y2, x1, x2 = c
            #         p = cv2.resize(p.astype(np.uint8), (x2 - x1, y2 - y1))
            #         f[y1:y2, x1:x2] = p
            #         q_infered.put(f)
        else:
            for img in full_frames:
                try:
                    result = win32file.WriteFile(video_pipe, img)
                    result = win32file.WriteFile(video_pipe, pcm)
                    print(result)
                except Exception as e:
                    print(e)
                    keep_running = False
                    break

if __name__ == '__main__':
    with open('config/config.yaml', 'r', encoding='utf-8') as fr:
        yaml_config = yaml.load(fr.read(), Loader=yaml.FullLoader)
    args = EasyDict(yaml_config)

    pipe_s_stream, pipe_r_stream = Pipe()
    pipe_s_stream_batch, pipe_r_stream_batch = Pipe()

    mgr = Manager()
    dict_flag = mgr.dict({'flag': False})

    lock = Lock()

    pipe_stream = Process(target=read_audio, args=(pipe_s_stream, 'read_audio'))
    pipe_stream.start()

    if pipe_stream.is_alive():
        pipe_stream_batch = Process(target=make_audio_batch, args=(args, pipe_r_stream, pipe_s_stream_batch, dict_flag, lock, 'make_audio_batch'))
        pipe_stream_batch.start()

        if pipe_stream_batch.is_alive():
            pipe_infered = Process(target=mel_2_lip_Q, args=(args, pipe_r_stream_batch, dict_flag, 'mel_2_lip'))
            pipe_infered.start()

    pipe_stream.join()
    print("read_audio exited!")

    pipe_infered.terminate()
    print("infer exited!")

    pipe_stream_batch.terminate()
    print("make_audio_batch exited!")