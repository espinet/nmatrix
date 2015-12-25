# = NMatrix
#
# A linear algebra library for scientific computation in Ruby.
# NMatrix is part of SciRuby.
#
# NMatrix was originally inspired by and derived from NArray, by
# Masahiro Tanaka: http://narray.rubyforge.org
#
# == Copyright Information
#
# SciRuby is Copyright (c) 2010 - 2014, Ruby Science Foundation
# NMatrix is Copyright (c) 2012 - 2014, John Woods and the Ruby Science Foundation
#
# Please see LICENSE.txt for additional copyright notices.
#
# == Contributing
#
# By contributing source code to SciRuby, you agree to be bound by
# our Contributor Agreement:
#
# * https://github.com/SciRuby/sciruby/wiki/Contributor-Agreement
#
# == fftw_spec.rb
#
# Tests for interfaces that are only exposed by nmatrix-fftw
#

require 'spec_helper'
require "./lib/nmatrix/fftw"

describe NMatrix::FFTW, focus: true do
  describe NMatrix::FFTW::Plan do
    context ".new" do
      it "creates a new plan for default DFT (complex input/complex output)" do
        plan = NMatrix::FFTW::Plan.new(4)
        # TODO: Figure a way to test internal C data structures.

        expect(plan.shape)    .to eq([4])
        expect(plan.size)     .to eq(4)
        expect(plan.dim)      .to eq(1)
        expect(plan.flags)     .to eq([:estimate])
        expect(plan.direction).to eq(:forward)
      end

      it "creates a new plan for multi dimensional DFT with options" do
        plan = NMatrix::FFTW::Plan.new([10,5,8],
          direction: :backward, flags: [:exhaustive, :estimate], dim: 3)

        expect(plan.shape)    .to eq([10,5,8])
        expect(plan.size)     .to eq(10*5*8)
        expect(plan.dim)      .to eq(3)
        expect(plan.flags)    .to eq([:exhaustive, :estimate])
        expect(plan.direction).to eq(:backward)
      end

      # it "creates a new plan for real input/complex output" do
      #   plan = NMatrix::FFTW::Plan.new([50,20,31,4,2],
      #     direction: :forward, flags: [:patient, :exhaustive], dim: 5, 
      #     type: :real_complex)

      #   expect(plan.shape) .to eq([50,20,31,4,2])
      #   expect(plan.size)  .to eq(50*20*31*4*2)
      #   expect(plan.dim)   .to eq(5)
      #   expect(plan.flags) .to eq([:patient, :exhaustive])
      #   expect(plan.type)  .to eq(:real_complex)
      # end

      it "creates a new plan for real input/real output" do
        pending "implement option :type => :r2r"
      end

      it "creates a new plan for complex input/real output" do
        pending "implement option :type => :c2r"
      end
    end

    context "#set_input" do
      it "accepts nothing but complex128 input for the default plan" do
        plan = NMatrix::FFTW::Plan.new(4)
        input = NMatrix.new([4], [23.54,52.34,52.345,64], dtype: :float64)

        expect {
          plan.set_input(input)
        }.to raise_error(ArgumentError)
      end

      it "accepts nothing but float64 input for real_complex or real_real plan" do
        plan = NMatrix::FFTW::Plan.new(4, type: :real_complex)
        input = NMatrix.new([4], [1,2,3,4], dtype: :int32)

        expect {
          plan.set_input(input)
        }.to raise_error(ArgumentError)
      end
    end

    context "#execute" do
      it "calculates a basic 1D DFT" do
        input = NMatrix.new([10],
          [
            Complex(9.32,0),
            Complex(44,0),
            Complex(125,0),
            Complex(34,0),
            Complex(31,0),
            Complex(44,0),
            Complex(12,0),
            Complex(1,0),
            Complex(53.23,0),
            Complex(-23.23,0),
          ], dtype: :complex128)

        output = NMatrix.new([10],
          [
            Complex(330.3200,0.0000),
            Complex(-8.4039  ,-150.3269),
            Complex(-99.4807 , -68.6579),
            Complex(-143.6861, -20.4273),
            Complex(67.6207  ,  8.5236),
            Complex(130.7800 ,  0.0000),
            Complex(67.6207  ,  -8.5236),
            Complex(-143.6861, 20.4273),
            Complex(-99.4807 , 68.6579),
            Complex(-8.4039  ,150.3269)
          ], dtype: :complex128)

        plan = NMatrix::FFTW::Plan.new(10)
        plan.set_input input
        expect(plan.execute).to eq(true)
        expect(plan.output.round(4)).to eq(output)
      end

      it "calculates 2D DFT with options" do
        input = NMatrix.new([2,2],
          [
            Complex(9.3200,0), Complex(43.0000,0),
            Complex(3.2000,0), Complex(4.0000,0)
          ], dtype: :complex128
        )

        output = NMatrix.new([2,2],
          [
            Complex(59.520,0), Complex(-34.480,0),
            Complex(45.120,0),  Complex(-32.880,0),
          ], dtype: :complex128
        )

        plan = NMatrix::FFTW::Plan.new([2,2],
          direction: :forward, flags: :estimate, dim: 2)
        plan.set_input input
        expect(plan.execute).to eq(true)
        expect(plan.output).to eq(output)
      end

      it "calculates ND DFT with options" do

      end

      it "calculates 1D real input/complex output DFT" do
        input  = NMatrix.new([4], [3.10, 1.73, 1.04, 2.83], dtype: :float64)
        output = NMatrix.new([3], 
          [Complex(8.70, 0), Complex(2.06, 1.1), Complex(-0.42, 0)])
        plan = NMatrix::FFTW::Plan.new([4], type: :real_complex)
        plan.set_input input
        expect(plan.execute).to eq(true)
        puts plan.output
        expect(plan.output).to eq(output)
      end

      # it "calculates 2D real input/complex output DFT" do
        
      # end
    end
  end

  context ".compute" do
    it "provides a DSL for neatly computing FFTs" do
      # DSL should take care of destruction of plan etc.
      pending "complete after rest of functions are done"
    end
  end
end
